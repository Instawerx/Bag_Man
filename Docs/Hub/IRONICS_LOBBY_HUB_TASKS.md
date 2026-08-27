# IRONICS_LOBBY_HUB_TASKS

**Companion to:** `IRONICS_LOBBY_HUB_SSOT.md` (why) · `IRONICS_LOBBY_HUB_ROADMAP.html` (order).
**Id range:** `AFL-30xx` (hub systems) · **`AFL-34xx` (the map — import, migrate, sanitise, assign, dress, optimise — in `IRONICS_HUB_MAP_BUILD_SPEC.md` §8)** · `AFL-32xx` (Track C, creator — `IRONICS_CC_INTEGRATION_PLAN.md` §6) · `AFL-33xx` (Track S, deferred). Existing ranges untouched.

**Critical path is the map:** AFL-3400 → 3401 → 3402 → 3403 (operator ratifies the zone assignment) → 3404 are the first five tickets after H0, in that order. AFL-3011 below is superseded by AFL-3401/3402.
**Lane key (amended 2026-08-26):** CC-W = Claude Code in a git worktree (no editor; parallel-safe) ·
CC-E = Claude Code through its direct editor connection (asset/placement/PIE work; one session at a
time) · CC-B = Claude Code in `Bag_Man_Backend` (parallel) · OP = Operator · CD = Claude Design ·
AIK = in-editor agent, **only** where the H0 capability probe shows the editor connection lacks the tool.
**Every task:** agent edits + shows diffs + stops; Operator builds (editor closed); proof is watched
in PIE, never "compiles". Code and content never share a commit.

Milestone names: `HUB-H0` … `HUB-H6`. Branch root: `feature/hub-<phase>-<short>`.

---

## H0 · READ AND FACT-LOCK

```
[AFL-3001] Execute clean-tree triage (owed)

Type:        Tech Debt      Discipline: Engineering     Priority: P0     Estimate: S
Sprint:      HUB-H0         Milestone:  HUB-H0          Branch:   n/a (tree hygiene)
Depends On:  None           Blocks:     AFL-3002..3005

## Context
The CC programme reported the triage table; the execute block was interrupted. No hub phase commits
onto a dirty tree, and D: server sessions require a quiescent tree.

## Acceptance Criteria
- [ ] Confirmed Config/code churn reverted (per the reported table, file by file, count stated first)
- [ ] `.codex/`, `Build/`, bridge files gitignored
- [ ] Operator answer recorded for stash@{0} (map/asset stash): keep-stashed | apply | drop
- [ ] `git status --porcelain` empty; recorded on the tracker with HEAD hash

## Technical Notes
Lane: OP decides on umap/uasset; CC-W executes reverts and gitignore edits on the editor checkout with the editor closed; CC never touches LFS
binaries without the operator's explicit per-file answer.

## Definition of Done
- [ ] Tracker row H0.1 written with evidence · [ ] Tree clean at a stated HEAD
```

```
[AFL-3002] HUB-READ-1 — world facts

Type:        Research       Discipline: Engineering     Priority: P0     Estimate: M
Sprint:      HUB-H0         Milestone:  HUB-H0          Branch:   n/a (read-only)
Depends On:  AFL-3001       Blocks:     AFL-3010, AFL-3011, AFL-3030

## Context
Every world-side [VERIFY] in the SSOT. Read-only; the output is a report with path:line per fact.

## Acceptance Criteria
- [ ] Military Mega Base Pack: present or absent on disk; if present, root path + demo map asset
      name + actor census (count by class; list every AI/sequence/ticking/physics-prop class)
- [ ] The hero `ULyraPawnData` asset the match experiences reference (path); its ability sets listed
- [ ] Lyra weapon spawner: class name, header path, key members (definition, cooldown, mesh, overlap)
- [ ] The interact verb: interface/class a world actor implements to be interactable by the proven
      grab/interact ability (path:line)
- [ ] The part-actor visibility path used by dismember hide (function, path:line) — reused by the
      club visibility mask
- [ ] The `#43` apply functions: facemask apply, MID push, AddCharacterPart entry points (path:line)
      — reused by client-local try-on

## Technical Notes
Tools: `rg`, `git ls-files`, `git lfs ls-files` for the pack. Fan out the six ACs to read-only
subagents, one per AC, then reconcile. Do not open the editor for this pass. Do not guess a path; a
fact without a path:line is reported as UNVERIFIED, not filled in.

**HUB-READ-0 (folded in, runs first): editor-connection capability probe.** Enumerate, by trying
each on a scratch asset and reverting: open map · place/delete/move actors · set actor/component
properties · create BP child / DataAsset / GE / MI · edit DataAsset rows · save · launch and stop PIE ·
read the log. The list of AIK-necessary operations for this programme is derived from what fails here,
and nothing else is assigned to AIK.

## Definition of Done
- [ ] `Docs/Hub/HUB-READ-1.md` committed alone (doc commit) · [ ] SSOT §12 rows 1,2,7,8 closed
```

```
[AFL-3003] HUB-READ-2 — door facts

Type:        Research       Discipline: Engineering     Priority: P0     Estimate: M
Sprint:      HUB-H0         Milestone:  HUB-H0          Branch:   n/a (read-only)
Depends On:  AFL-3001       Blocks:     AFL-3014, AFL-3020..3023

## Acceptance Criteria
- [ ] Every AFL ability that fires a weapon: class, file, current `ActivationBlockedTags` value
- [ ] League/Staked door widget: class, how it is pushed today (cheat and/or menu path), what it
      calls on confirm (FlexMatch client entry point, path:line)
- [ ] Match-end client path: what the client does when a match ends (function, path:line); where
      "return to front end" lives
- [ ] `afl.Store.Open` push site (path:line) and the widget class it pushes
- [ ] Loadout entry: widget class + how it opens; `PreviewRT` SceneCapture site
      (`AFLW_LoadoutBase.cpp:481` confirmed or corrected)
- [ ] Product-page widget from the three-lane design pipeline: exists? class? state?
- [ ] `ClientRequestPurchase` call sites (`AFLW_FrontEndMarket.cpp:476,483,904,906` confirmed or
      corrected)

## Definition of Done
- [ ] `Docs/Hub/HUB-READ-2.md` committed alone · [ ] SSOT §12 rows 3,4,5,11 closed
```

```
[AFL-3004] HUB-READ-3 — online facts

Type:        Research       Discipline: Engineering     Priority: P0     Estimate: S
Sprint:      HUB-H0         Milestone:  HUB-H0          Branch:   n/a (read-only)
Depends On:  AFL-3001       Blocks:     AFL-3040..3045, AFL-3050..3053

## Acceptance Criteria
- [ ] EOS-AUTH-C2 status on disk (config present? committed? last log evidence?)
- [ ] EOS OSS lobby + friends interfaces available in the current engine/plugin set (`IOnlineLobby`
      or OnlineServices equivalent — name what the build actually has)
- [ ] `Bag_Man_Backend`: start-matchmaking handler request shape; whether a multi-player ticket is
      already supported; `BagManTentpoleStack` fleet/queue definitions (OS, instance family, max
      players, aliases)
- [ ] Existing player-session creation path (client side) reused by hub join

## Definition of Done
- [ ] `Docs/Hub/HUB-READ-3.md` committed alone · [ ] SSOT §12 rows 6,9,10 closed
```

```
[AFL-3005] Record operator rulings (SSOT §11)

Type:        Research       Discipline: Design          Priority: P0     Estimate: XS
Sprint:      HUB-H0         Milestone:  HUB-H0          Branch:   n/a
Depends On:  AFL-3002..3004 Blocks:     AFL-3010

## Acceptance Criteria
- [ ] Shard model @64 · asset names · chat model · club fallback · first-beta Disabled doors — each
      recorded as ruled or default-taken, on the tracker H0.5 row
```

---

## H1 · HUB SPINE

```
[AFL-3010] AFLHub GameFeature shell + tag declarations

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: S
Sprint:      HUB-H1         Milestone:  HUB-H1          Branch:   feature/hub-h1-spine
Depends On:  AFL-3005       Blocks:     AFL-3012..3016

## Context
All hub code lives here. Nothing hub-shaped goes into a match GameFeature or `/Game`.

## Acceptance Criteria
- [ ] `Plugins/GameFeatures/AFLHub/` with uplugin (GameFeatureType GameFeature), `AFLHub.Build.cs`
      depending on LyraGame, GameplayAbilities/Tags/Tasks, ModularGameplay, CommonUI, the AFL cosmetic
      module(s) named in HUB-READ-1, `AFLNetTypes`, `AFLVFX`
- [ ] `Config/Tags/AFLHubTags.ini` declaring `Hub.Zone.{Main,Range,PX,Labs,Barracks,Cafe,EMLounge,
      Deploy,Tournament,MiniGame,Assigned}` and `Hub.Restriction.NoFire`
- [ ] Module compiles as part of the operator's editor-closed UBT build; zero new warnings
- [ ] No `/Game` reference from the plugin

## Technical Notes
Conform to an existing AFL GameFeature's uplugin/Build.cs (pick the newest proven one from disk and
diff against it). Tags for the hub are declared here, never in the combat tag ini.

## Definition of Done
- [ ] Diff shown, operator built, Result: Succeeded · [ ] Code-only commit
```

```
[AFL-3011] Import, sanitise, rename the Mega Base demo map → L_AFL_OutpostEarth   → SUPERSEDED by AFL-3400..3402 (IRONICS_HUB_MAP_BUILD_SPEC.md §8)

Type:        Pipeline       Discipline: Art             Priority: P0     Estimate: M
Sprint:      HUB-H1         Milestone:  HUB-H1          Branch:   feature/hub-h1-map
Depends On:  AFL-3002       Blocks:     AFL-3013, AFL-3015

## Acceptance Criteria
- [ ] Map lives under `AFLHub/Content/Maps/L_AFL_OutpostEarth` (not `/Game`)
- [ ] Deleted: every demo AI BP, ticking target actor, level sequence, physics prop (from the
      HUB-READ-1 census; count stated before deletion, count after)
- [ ] PlayerStarts placed in the Main Zone; the 11 zone footprints marked with placeholder volumes
      (unwired) so H1.6/H2.1 place real volumes on agreed footprints
- [ ] Map opens, plays in PIE with the default experience with no error log lines from removed classes

## Technical Notes
Lane CC-E. Operator imports from Fab first if HUB-READ-1 found the pack absent. Do not merge/HISM yet
(H5). Content-only commit from the editor checkout, LFS.

## Definition of Done
- [ ] Before/after actor counts on the tracker · [ ] Content-only commit
```

```
[AFL-3012] UAFLHubNetProfileComponent

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: M
Sprint:      HUB-H1         Milestone:  HUB-H1          Branch:   feature/hub-h1-spine
Depends On:  AFL-3010       Blocks:     AFL-3013, AFL-3017

## Context
Delivers the helper doc's 10–15 Hz / quantised / zone-culled targets on the Lyra hero pawn with engine
facilities (SSOT §2.2). No custom net struct; P-CONTROLS doctrine (GameFeature-attached
UActorComponent, no pawn subclass).

## Acceptance Criteria
- [ ] Attached via `GameFeatureAction_AddComponents` in the hub experience only
- [ ] On `BeginPlay` (authority + owning client): `NetUpdateFrequency=15`, `MinNetUpdateFrequency=5`;
      `ReplicatedMovement` quantisation Location=RoundOneDecimal, Rotation=ByteComponents,
      Velocity=RoundWholeNumber
- [ ] Listens for `Hub.Zone.*` tag add/remove on the owner ASC and sets `NetCullDistanceSquared` from
      `DA_AFL_HubZoneProfiles` (row per zone: cull distance, mirror capture res/rate) — data, not code
- [ ] Values are `EditDefaultsOnly` with the above as defaults; no magic numbers in cpp
- [ ] Compiles in Shipping (no dev-only includes)

## Technical Notes
Zone tag listening: `AbilitySystemComponent->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::
NewOrRemoved)` per zone tag from the profile asset. Do not touch CMC. Structurally diff against the
proven GameFeature-attached component used for CMC mechanics (P-CONTROLS) and justify each divergence.

## Definition of Done
- [ ] Diff + Proven-Sibling justification shown · [ ] Operator build Succeeded · [ ] Code-only commit
```

```
[AFL-3013] B_AFL_Experience_Hub + hub ability set + hub HUD layout

Type:        Feature        Discipline: Design          Priority: P0     Estimate: M
Sprint:      HUB-H1         Milestone:  HUB-H1          Branch:   feature/hub-h1-map
Depends On:  AFL-3011, AFL-3012   Blocks: AFL-3016, AFL-3017

## Acceptance Criteria
- [ ] `ULyraExperienceDefinition` asset using the hero `ULyraPawnData` found in HUB-READ-1
- [ ] Actions: AddAbilities → `DA_AFL_AbilitySet_Hub` (movement, interact, dash; the loadout
      AbilitySet so weapons are carried; NO energy/extraction/lag-comp/dismember-only sets);
      AddComponents → `UAFLHubNetProfileComponent` on the hero pawn; AddWidgets → hub HUD layout
      (zone prompt slot; nameplate + chat slots empty until H4)
- [ ] The infinite GE granting `Hub.Restriction.NoFire` applied on possession via the ability set's
      granted effects (fail-closed from frame 0)
- [ ] `L_AFL_OutpostEarth` default experience = this asset
- [ ] Structural diff against the match experience asset: every divergence listed and justified

## Technical Notes
Lane CC-E. Experience lives inside `AFLHub` (experiences referencing GameFeature content belong in a
GameFeature). Do not open or edit any match experience asset.

## Definition of Done
- [ ] Diff table on the tracker · [ ] Content-only commit
```

```
[AFL-3014] Fail-closed fire block — one ActivationBlockedTags entry per fire ability

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: S
Sprint:      HUB-H1         Milestone:  HUB-H1          Branch:   feature/hub-h1-fireblock
Depends On:  AFL-3003, AFL-3010   Blocks: AFL-3015

## Context
The only edit this programme makes to combat code. One line per ability. In a match the tag never
exists, so match fire is unchanged by construction.

## Acceptance Criteria
- [ ] Every ability in the HUB-READ-2 fire list gains `Hub.Restriction.NoFire` in
      `ActivationBlockedTags` (constructor or CDO — match what each file already does)
- [ ] Diff is exactly N files × ≤3 changed lines; no other edit
- [ ] Match regression proof (2-client PIE, live Pulse → damage, beam → damage) re-run and green
- [ ] Hub: with the GE active, fire input produces no ability activation on server or client (log)

## Technical Notes
Proven-Sibling: the Pulse ability is the reference; the diff for each other ability must be the same
shape. If any fire ability is BP-only, the tag goes on the BP CDO (CC-E) and is listed as such.

## Definition of Done
- [ ] Diffs shown · [ ] Operator build · [ ] Regression proof cited on the tracker · [ ] Code commit
      separate from any BP/content commit
```

```
[AFL-3015] AAFLHubZoneVolume + one zone canary (Main → PX)

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: L
Sprint:      HUB-H1         Milestone:  HUB-H1          Branch:   feature/hub-h1-zones
Depends On:  AFL-3011, AFL-3013, AFL-3014   Blocks: AFL-3017, AFL-3020

## Acceptance Criteria
- [ ] Actor with a box/brush component; `ZoneTag` (FGameplayTag). No fire-authorisation role — fire is
      authorised by experience (SSOT §2.3); the Shooting Range is a separate map (AFL-3025)
- [ ] Server-authority only: overlap begin applies an infinite GE granting `ZoneTag` to the pawn's ASC
      (`ULyraAbilitySystemComponent`); overlap end removes it. Client overlaps do nothing.
- [ ] Idempotent across possession double-fire and respawn inside a volume (re-evaluates on
      `OnPossessed`/pawn change; no stacked GEs — assert count ≤1 in a dev-only check)
- [ ] One PX volume placed (CC-E) as the canary; the net profile swaps cull distance on the tag
- [ ] Dev-only log marker `AFL_HUB[Zone] enter|exit tag=…` for the automated PIE harness

## Technical Notes
Conform to the extraction-zone actor's overlap/authority pattern (proven, EXTRACTION-CYCLE1); cite
its file when diffing. GE assets: `GE_AFL_Hub_Zone` (SetByCaller tag) and `GE_AFL_Hub_NoFire` (applied by the hub
experience only, never by a volume) under `AFLHub/Content/Effects` — CC-E authors after the CC-W code lands, CC-W references by soft class.

## Definition of Done
- [ ] Code commit + separate content commit · [ ] Log ledger from a 2-client run attached
```

```
[AFL-3016] Hub HUD — zone prompt widget

Type:        Feature        Discipline: Design          Priority: P1     Estimate: S
Sprint:      HUB-H1         Milestone:  HUB-H1          Branch:   feature/hub-h1-map
Depends On:  AFL-3013       Blocks:     None

## Acceptance Criteria
- [ ] `AFLW_HubZonePrompt` (CommonUserWidget) on the `UI.Layer.Game` slot of the hub HUD layout
- [ ] Shows the zone display name on `Hub.Zone.*` tag change (client-side ASC tag listener); fades
- [ ] Brand tokens only (Orbitron/NotoSans; `#1E5AFF` accent). No cyan accent.

## Definition of Done
- [ ] Content-only commit · [ ] Watched in PIE
```

```
[AFL-3017] Dedicated-server hub run — H1 proof

Type:        Pipeline       Discipline: QA              Priority: P0     Estimate: M
Sprint:      HUB-H1         Milestone:  HUB-H1          Branch:   n/a
Depends On:  AFL-3012..3016 Blocks:     H2

## Acceptance Criteria (the H1 gate)
- [ ] D: `LyraServer` build, `-log` run on `L_AFL_OutpostEarth`; C: launcher `LyraEditor` rebuilt
      afterwards (two-engine rule)
- [ ] Two clients connect by IP (dev accounts `AFL_DEV_TEST_01/02`); each renders the other's robot
      body, facemask, finish, carried weapon (operator-watched on both windows)
- [ ] Fire refused on both clients everywhere in the hub; crossing the canary volume swaps the zone
      tag on both (log ledger: `AFL_HUB[Zone]` + ability-refused lines)
- [ ] Match regression proof re-run green (AFL-3014 AC)
- [ ] Tracker H1 rows written with hashes; tree clean; tags after push

## Technical Notes
CC-W assembles the launch commands and the log ledger; OP runs the editor-checkout build and watches. Nothing in this
task is proven by "it connected".
```

---

## H2 · DOORS

```
[AFL-3020] AAFLHubDestinationVolume + DA_AFL_HubDestinations

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: L
Sprint:      HUB-H2         Milestone:  HUB-H2          Branch:   feature/hub-h2-doors
Depends On:  AFL-3015, AFL-3003   Blocks: AFL-3021..3025

## Acceptance Criteria
- [ ] Data asset: `TMap<FName DestinationId, FAFLHubDestination{EAction, TSoftClassPtr<UCommonActivatable
      Widget> Widget, FGameplayTag Layer, FName PreFocusSkuId, TSoftObjectPtr<UWorld> Map,
      FPrimaryAssetId ExperienceId, bool bReturnToHub, FName ClubFlavour, FText DisabledPrompt}>`
- [ ] Volume: owning-client overlap begin → prompt; interact (existing verb) → resolve action
- [ ] `PushWidget`: `UCommonUIExtensions::PushContentToLayer_ForPlayer` on the configured layer
      (the `afl.Store.Open` pattern, cited from HUB-READ-2); pre-focus SKU passed via the widget's
      existing focus API if present, else a documented additive setter
- [ ] `ExperienceTravel`: the Lyra experience travel path the match uses (cited); options carry
      `ReturnToHub=1`
- [ ] `JoinClub`: calls the club component (H4); until H4 → behaves as `Disabled`
- [ ] `Disabled`: shows `DisabledPrompt`, no action. **Default for any row with an unproven backend.**
- [ ] All 11 destination volumes placed (CC-E) on the H1 footprints; rows authored for each

## Technical Notes
No door is allowed to stub a backend. If a widget class is not on disk, the row is `Disabled`.

## Definition of Done
- [ ] Code commit + content commit · [ ] Every row's action watched at least once in PIE
```

```
[AFL-3021] Deployments door → existing League/Staked door surface

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: M
Sprint:      HUB-H2         Milestone:  HUB-H2          Branch:   feature/hub-h2-doors
Depends On:  AFL-3020       Blocks:     AFL-3022, AFL-3026

## Acceptance Criteria
- [ ] Row `Deploy` = `PushWidget` of the existing door widget class (HUB-READ-2); no new widget
- [ ] On confirm, the existing FlexMatch flow runs unchanged; match found → client disconnects from
      the hub and travels to the match server exactly as the front end does today
- [ ] Zero edits to the door widget, FlexMatch client code, rule sets, queues, SSM
- [ ] Solo match completes from a hub-initiated queue (dedicated hub + real queue)

## Definition of Done
- [ ] `git diff --stat` shows no file under the matchmaking/door paths changed · [ ] Watched
```

```
[AFL-3022] Return-to-hub

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: M
Sprint:      HUB-H2         Milestone:  HUB-H2          Branch:   feature/hub-h2-return
Depends On:  AFL-3003, AFL-3021   Blocks: AFL-3026, AFL-3053

## Acceptance Criteria
- [ ] The match-end client path (HUB-READ-2) routes to `UAFLHubJoinSubsystem::RequestHubJoin()`
      (new, `AFLHub`) when the session was entered from the hub (`ReturnToHub` flag persisted on the
      client's game instance across travel); otherwise unchanged behaviour
- [ ] H2 implementation: dev direct-connect (address from a console variable / `-ini` override);
      H5 replaces the resolver with the join service — same subsystem API, no caller change
- [ ] Cosmetic selection intact after return (persisted read-back, not carried in memory)

## Technical Notes
Additive: one branch at the existing match-end site, guarded by the flag. Cite the site.

## Definition of Done
- [ ] Diff shown · [ ] Match → hub watched on dedicated servers · [ ] Code commit
```

```
[AFL-3023] Labs / Barracks / PX rows (existing widgets)

Type:        Feature        Discipline: Design          Priority: P1     Estimate: S
Sprint:      HUB-H2         Milestone:  HUB-H2          Branch:   feature/hub-h2-doors
Depends On:  AFL-3020       Blocks:     H3

## Acceptance Criteria
- [ ] `Labs` → CC shell class if on disk (CC-5.1) else `Disabled` ("Robo Labs opening soon")
- [ ] `Barracks` → the loadout widget class (HUB-READ-2)
- [ ] `PX` → the store widget class (`afl.Store.Open` target) — H3 replaces with the product page
- [ ] Each opens and closes cleanly from the hub with correct input mode (menu) and returns game input

## Definition of Done
- [ ] Content-only commit · [ ] Watched
```

```
[AFL-3024] Landing Page

Type:        Feature        Discipline: Design          Priority: P0     Estimate: L
Sprint:      HUB-H2         Milestone:  HUB-H2          Branch:   feature/hub-h2-landing
Depends On:  AFL-3003, AFL-3022   Blocks: AFL-3026

## Context
Operator flow: Sign in / Sign up · 3D map-shot backdrop · New player → wallet with free starters ·
Return player → last set character · Enter Base.

## Acceptance Criteria
- [ ] Front-end map stays the cold-boot entry; its default experience pushes `AFLW_Landing`
      (CommonActivatableWidget, `UI.Layer.Menu`)
- [ ] Sign in / Sign up drive the existing identity path (IRONICS accounts; EOS OAuth when C2 is
      green) — no new auth code in the widget
- [ ] Backdrop: a camera on a lightweight `L_AFL_OutpostEarth_Backdrop` sublevel (streamed, low LOD,
      no pawns) or a captured still — CC-E picks by measured load time (<3 s cold) and records which
- [ ] New player: first wallet read triggers `GrantedFree` auto-ownership (existing); the widget shows
      the starter set from the owned-set, not a hardcoded list
- [ ] Return player: last-set character shown from the persisted selection read-back
- [ ] Enter Base → `UAFLHubJoinSubsystem::RequestHubJoin()`
- [ ] Copy: `IRONICS` wordmark; hero copy tokens from the brand (no invented strings); Claude Design
      supplies the visual layer via the three-lane pipeline

## Definition of Done
- [ ] Content commit (widget + sublevel) separate from any code commit · [ ] Watched cold boot
```

```
[AFL-3025] Shooting Range map — the travel canary   (operator ruling: the Range is a separate map)

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: L
Sprint:      HUB-H2         Milestone:  HUB-H2          Branch:   feature/hub-h2-range
Depends On:  AFL-3020, AFL-3022   Blocks: AFL-3026, AFL-3060

## Acceptance Criteria
- [ ] `L_AFL_ShootingRange` **(proposed)** under `AFLHub/Content/Maps` (greybox or a partition of the
      Mega Base pack — CC-E picks from what HUB-READ-1 found; recorded)
- [ ] `B_AFL_Experience_Range`: hero pawn data, full loadout AbilitySet, **no** `GE_AFL_Hub_NoFire`;
      structurally diffed against `B_AFL_Experience_Hub` — the only justified divergences are the
      missing NoFire GE and the combat ability sets needed to fire
- [ ] Existing LagTest/DamageTarget dummies placed on the range
- [ ] Hub door row `Range` = `ExperienceTravel` with `ReturnToHub=1`; an exit door on the range
      (another `AAFLHubDestinationVolume` travelling back via the join subsystem)
- [ ] Proof: hub (fire refused) → Range door → range (fire lands on a dummy, both clients) → exit →
      hub (fire refused again); no cheat

## Technical Notes
Replaces the "any existing map" canary — the Range proves travel, fire-by-experience, and return in
one loop. Range scoring/boards are Track S (deferred), not this ticket.

[AFL-3025b] Tournaments / Mini Games / Assigned Match rows

Type:        Feature        Discipline: Design          Priority: P2     Estimate: S
Sprint:      HUB-H2         Milestone:  HUB-H2          Branch:   feature/hub-h2-doors
Depends On:  AFL-3025       Blocks:     AFL-3060

## Acceptance Criteria
- [ ] Rows authored as `ExperienceTravel` if a partition/mini map exists on disk (HUB-READ-1), else
      `Disabled` with prompt text; recorded per row

## Definition of Done
- [ ] Content-only commit · [ ] Canary watched
```

```
[AFL-3026] H2 gate proof — the full loop, no cheats

Type:        Pipeline       Discipline: QA              Priority: P0     Estimate: M
Sprint:      HUB-H2         Milestone:  HUB-H2          Branch:   n/a
Depends On:  AFL-3020..3025 Blocks:     H3, H4, H5

## Acceptance Criteria
- [ ] Cold boot → Landing → Enter Base → hub (dedicated) → Range door → range: fire lands on a dummy
      on both clients → exit → hub: fire refused → Deployments → real solo FlexMatch match → match
      ends → hub; cosmetics intact; Labs/Barracks/PX open their widgets
- [ ] Zero console commands in the run (log audited)
- [ ] Match regression proof re-run green
- [ ] Tracker H2 rows; tree clean; tags
```

---

## H3 · PX STORE AND BARRACKS

```
[AFL-3030] AAFLDisplayPedestal — weapon-spawner child, recoloured

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: L
Sprint:      HUB-H3         Milestone:  HUB-H3          Branch:   feature/hub-h3-retail
Depends On:  AFL-3002, AFL-3026   Blocks: AFL-3031..3036

## Context
Operator ruling: reuse the map weapon spawner system recoloured for store and loadout display.

## Acceptance Criteria
- [ ] Subclass of the Lyra weapon spawner class (HUB-READ-1); grant-on-overlap disabled; cooldown
      removed; `SkuId` (FName) property; display mesh resolved from the catalog row's display asset
      (soft, async) — if the row has none, the pedestal shows the pad only and logs once
- [ ] Spinning display + pad retained; pad MI re-instanced: base `#222A3A`, emissive `#1E5AFF`;
      `#FF00D5` variant for premium/staked rows (data flag)
- [ ] Implements the interact verb interface (HUB-READ-1); interact → `AAFLHubPreviewAnchor` blend +
      product page pre-focused (AFL-3032/3033)
- [ ] Not replicated beyond the base spawner's existing replication; no per-player state on the actor
- [ ] Canary: one pedestal, `AFL.Facemask.Flag.Japan`

## Technical Notes
Proven-Sibling: diff against the Lyra spawner; every removed/added member justified. Client-side
line traces for interaction use `ECC_HubRetail` (AFL-3037).

## Definition of Done
- [ ] Code commit · [ ] Canary placed (content commit) · [ ] Watched
```

```
[AFL-3031] AAFLHubPreviewAnchor + camera blend + local rotate   → DELIVERED BY TRACK C (AFL-3211)

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: M
Sprint:      HUB-C1         Milestone:  HUB-C1          Branch:   feature/cc-c1-preview
Depends On:  AFL-3201       Blocks:     AFL-3033, AFL-3220

## Acceptance Criteria
- [ ] Actor with a camera component; `SetViewTargetWithBlend(Anchor, 0.4f, VTBlend_Cubic)` in,
      blend back to the pawn's camera on exit; owning client only
- [ ] `SpawnClientOnlyPreviewInstance(SkuId)`: non-replicated local instance (weapon/robot/mask) at
      the anchor's preview transform; destroyed on exit
- [ ] Mouse-drag / right-stick rotates the preview instance about its pivot; input routed through
      the CommonUI input config of the product page (menu mode, no game input leak)
- [ ] Movement/interact disabled while anchored; restored on exit; exiting the PX zone forces exit

## Definition of Done
- [ ] Code commit · [ ] Watched on both anchor entry and every exit path
```

```
[AFL-3032] Product page pre-focus API (additive)   → FOLDED INTO TRACK C5 (AFL-3250)

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: S
Sprint:      HUB-C5         Milestone:  HUB-C5          Branch:   feature/cc-c5-productpage
Depends On:  AFL-3223       Blocks:     AFL-3033

## Acceptance Criteria
- [ ] The product-page widget (design-pipeline output, HUB-READ-2) exposes `SetFocusedSku(FName)`;
      if it already has a focus entry point, use it and add nothing
- [ ] No other change to the widget in this ticket
```

```
[AFL-3033] Pedestal → product page → ClientRequestPurchase → equip   → FOLDED INTO TRACK C5 (AFL-3250)

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: M
Sprint:      HUB-C5         Milestone:  HUB-C5          Branch:   feature/cc-c5-productpage
Depends On:  AFL-3030, AFL-3211, AFL-3223   Blocks: AFL-3038

## Context
One purchase path. This ticket also closes the owed "no store WidgetBP calls ClientRequestPurchase"
item.

## Acceptance Criteria
- [ ] Buy on the product page calls `UAFLWalletComponent::ClientRequestPurchase` — the same call the
      front-end market uses; no PlayFab SDK call from any hub class (grep-enforced in review)
- [ ] Equip after purchase → `ServerSetCosmeticSelection` on the `#43` seam
- [ ] Canary: buy `Flag.Japan` at the pedestal on client A → deduct → grant → entitled → equip → visor
      swaps on A and on B; survives A respawn (Phase-2 flow, now spatial)
- [ ] Insufficient funds / not transactable: existing declines surface on the page (no new strings)

## Definition of Done
- [ ] Diff shown · [ ] 2-client dedicated proof on the tracker · [ ] Code commit
```

```
[AFL-3034] UAFLCosmeticPreviewComponent — client-local try-on / hold   → DELIVERED BY TRACK C (AFL-3210)

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: L
Sprint:      HUB-C1         Milestone:  HUB-C1          Branch:   feature/cc-c1-preview
Depends On:  AFL-3002, AFL-3201   Blocks: AFL-3035, AFL-3220, AFL-3038

## Context
"Try on or hold" with 0% server cost. The preview is the product: it calls the same apply functions
the authoritative selection calls, on the local pawn only.

## Acceptance Criteria
- [ ] Attached to the hero pawn in the hub experience only; `IsLocallyControlled()` guard on every
      entry point; no UFUNCTION(Server/Client/NetMulticast); nothing replicated
- [ ] `BeginPreview(SkuId)` resolves the row's axis and calls the HUB-READ-1-cited apply function for
      that axis (facemask apply / MID colour push / AddCharacterPart for robot & accessory / weapon
      mesh socket "hold"); `EndPreview()` restores from the authoritative `FAFLCosmeticSelection`
      by re-running the normal apply, never from a cached copy
- [ ] Auto-`EndPreview()` on: PX zone exit, selection replication (`OnRep`) arriving, pawn unpossess,
      death
- [ ] Never writes `FAFLCosmeticSelection`; never calls `ServerSetCosmeticSelection`
- [ ] Dev-only log `AFL_HUB[Preview] begin|end sku=…` for the harness

## Technical Notes
Structurally diff against `UAFLCharacterPartSelectorComponent`'s idempotent replace (RemoveAll first)
for the robot/accessory axis. Any apply function that is server-only today gets an additive
client-callable overload in its own small commit, cited.

## Definition of Done
- [ ] Diff shown · [ ] 2-client proof: B sees no change during A's preview · [ ] Code commit
```

```
[AFL-3035] AAFLHubMirror   → DELIVERED BY TRACK C (AFL-3212)

Type:        Feature        Discipline: Engineering     Priority: P1     Estimate: M
Sprint:      HUB-C1         Milestone:  HUB-C1          Branch:   feature/cc-c1-preview
Depends On:  AFL-3034       Blocks:     AFL-3038

## Acceptance Criteria
- [ ] `USceneCaptureComponent2D` with `bCaptureEveryFrame=false` default, RT sized from
      `DA_AFL_HubZoneProfiles`
- [ ] Front trigger: on overlap by the **locally controlled** pawn only → `bCaptureEveryFrame=true`,
      `PrimitiveRenderMode=PRM_UseShowOnlyList`, show-only = local pawn + its part actors + preview
      instance; on exit → off
- [ ] Capture rate throttled to the profile value (not every frame at 60)
- [ ] Mirror surface material uses the RT; two mirrors on the map (jewellery counter, mask wall)

## Definition of Done
- [ ] Code commit + content commit · [ ] Watched; frame cost recorded before/after
```

```
[AFL-3036] AAFLDisplayRack — catalog-driven pedestal spawn

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: M
Sprint:      HUB-H3         Milestone:  HUB-H3          Branch:   feature/hub-h3-retail
Depends On:  AFL-3030       Blocks:     AFL-3038

## Acceptance Criteria
- [ ] Properties: `FAFLCatalogFilter` (type / axis / collection / rarity / `bOwnedByLocalPlayerOnly`),
      `TArray<FTransform> SpawnSlots`, pedestal class
- [ ] `BeginPlay` (server): query the catalog subsystem (HUB-READ-1 name), spawn one pedestal per
      matching row into slots in order; overflow rows logged once, not spawned
- [ ] Barracks racks: `bOwnedByLocalPlayerOnly` resolves client-side against the wallet owned-set
      (pedestals spawn on the server for all rows; visibility filtered locally) — no per-player server
      spawning
- [ ] Racks placed (CC-E): weapons wall, mask wall, robots; jewellery counter (Accessory) populates
      when Track C6 (AFL-3260) lands; sticker rack row `Disabled` until CC-7
- [ ] Barracks pedestal interact opens the Track C4 loadout shell (AFL-3240)
- [ ] New catalog row → appears on next map load with no map edit (proven by adding one dev row)

## Definition of Done
- [ ] Code commit + content commit · [ ] Watched
```

```
[AFL-3037] ECC_HubRetail trace channel

Type:        Tech Debt      Discipline: Engineering     Priority: P2     Estimate: XS
Sprint:      HUB-H3         Milestone:  HUB-H3          Branch:   feature/hub-h3-retail
Depends On:  AFL-3010       Blocks:     None

## Acceptance Criteria
- [ ] Custom object channel declared in `DefaultEngine.ini` collision profiles; shelving/pedestal
      meshes assigned (CC-E); client interaction traces use it; pawns ignore it
```

```
[AFL-3038] H3 gate proof + old store/loadout entry cutover

Type:        Pipeline       Discipline: QA              Priority: P0     Estimate: M
Sprint:      HUB-H3         Milestone:  HUB-H3          Branch:   feature/hub-h3-cutover
Depends On:  AFL-3030..3037, AFL-3231 (C3 gate), AFL-3240, AFL-3250   Blocks: H6

## Acceptance Criteria
- [ ] Roadmap H3 PROOF watched on dedicated + 2 clients; match proof re-run
- [ ] Only then: the front-end store and loadout **entry points** removed (menu buttons / cheats
      that opened them); the widgets themselves untouched
- [ ] Tracker rows; tree clean; tags
```

---

## H4 · SOCIAL

```
[AFL-3040] Nameplates + presence widget

Type:        Feature        Discipline: Design          Priority: P1     Estimate: M
Sprint:      HUB-H4         Milestone:  HUB-H4          Branch:   feature/hub-h4-social
Depends On:  AFL-3026       Blocks:     None

## Acceptance Criteria
- [ ] World-space nameplate per visible pawn (display name from `ALyraPlayerState`); friend and club
      markers; distance fade; pooled widgets, budget ≤64 live
```

```
[AFL-3041] UAFLHubChatComponent + shared profanity filter

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: L
Sprint:      HUB-H4         Milestone:  HUB-H4          Branch:   feature/hub-h4-chat
Depends On:  AFL-3026       Blocks:     AFL-3043

## Acceptance Criteria
- [ ] On `ALyraPlayerState`; `ServerSendChat(Channel, Text)` validates rate (token bucket, data
      values), length, and profanity via a new `UAFLTextFilterSubsystem` shared with CC-5.4
- [ ] Delivery: server iterates recipients by channel — Proximity (same `Hub.Zone.*` tag OR within
      `ProximityRadius`), Club (same ClubId), Party — and calls `ClientReceiveChat` per recipient (no
      NetMulticast; relevancy-bounded fan-out)
- [ ] Chat widget on the hub HUD (CC-E); channel tabs; input mode handled via CommonUI
- [ ] Dev-only markers for the harness; 3-client proof: proximity message unseen by a far client

## Technical Notes
No net struct: FString + uint8 channel over the RPC. Filter subsystem lives in an always-loaded
module so CC can use it.
```

```
[AFL-3042] UAFLHubClubComponent + visibility mask (no EOS dependency)

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: L
Sprint:      HUB-H4         Milestone:  HUB-H4          Branch:   feature/hub-h4-clubs
Depends On:  AFL-3020, AFL-3002   Blocks: AFL-3043, AFL-3044

## Acceptance Criteria
- [ ] On `ALyraPlayerState`: replicated `FName ClubId`, `EAFLClubPrivacy` {Public, FriendsOnly,
      InviteOnly}, `FName ClubFlavour` (Cafe | EMLounge)
- [ ] Server-auth `ServerCreateClub / ServerJoinClub / ServerLeaveClub` from the lounge door
      (`JoinClub` action); join denied by privacy on the server
- [ ] Local mask: when the local player is inside a lounge zone with an active ClubId, pawns with a
      different ClubId are hidden via the part-actor visibility path (HUB-READ-1) — never destroyed,
      never un-replicated; unmask on lounge exit
- [ ] 3-client proof per the roadmap H4 gate
```

```
[AFL-3043] EOS lobby binding — Café / EM Lounge, friends, invites, voice   [GATED: EOS-AUTH-C2]

Type:        Feature        Discipline: Engineering     Priority: P1     Estimate: XL
Sprint:      HUB-H4         Milestone:  HUB-H4          Branch:   feature/hub-h4-eos
Depends On:  AFL-3042, AFL-3004 (C2 green)   Blocks: AFL-3044

## Acceptance Criteria
- [ ] `ServerCreateClub` also creates an EOS lobby (interface named in HUB-READ-3) with the privacy
      mapped to `FriendsOnly` / `InviteOnly`; lobby id stored beside ClubId
- [ ] Friends list + invite from the nameplate/club UI; accepted invite → `ServerJoinClub`
- [ ] EOS RTC voice enabled for club members
- [ ] Until C2 is green this ticket does not start; AFL-3042 ships without it
```

```
[AFL-3044] Party deploy — FlexMatch multi-player ticket

Type:        Feature        Discipline: Engineering     Priority: P0     Estimate: L
Sprint:      HUB-H4         Milestone:  HUB-H4          Branch:   feature/hub-h4-party (game) · feature/party-ticket (backend)
Depends On:  AFL-3021, AFL-3042, AFL-3004   Blocks: AFL-3045

## Acceptance Criteria
- [ ] Backend (`Bag_Man_Backend`, own commit): start-matchmaking handler accepts an optional
      `partyPlayerIds[]`; submits one FlexMatch ticket with all players; no rule set / queue / SSM change
- [ ] Client: party leader's Deploy confirm passes party ids; ticket id written to the club/lobby
      attributes (EOS when bound, else replicated on the club component); members observe and follow
      to the match with their own player sessions
- [ ] Two dev accounts land in the same real match and both return to the hub
```

```
[AFL-3045] H4 gate proof
Type: Pipeline · Discipline: QA · Priority: P0 · Estimate: S · Sprint: HUB-H4
Roadmap H4 PROOF watched; match proof re-run; tracker; clean tree; tags.
```

---

## H5 · SCALE AND INFRASTRUCTURE

```
[AFL-3050] Map optimisation pass
Type: Polish · Discipline: Art · Priority: P1 · Estimate: L · Sprint: HUB-H5 · Lane: CC-E + OP (AIK only if Merge Actors is not drivable over the connection)
- [ ] HISM merge of prop clusters (Merge Actors); draw calls before/after on the tracker
- [ ] NavMesh bounds shrunk to walkable surfaces between zones
- [ ] World Partition cell sizing / streaming budget; cold-load and zone-crossing hitch measured
```

```
[AFL-3051] POST /hub/join Lambda + HubPresence table   (Bag_Man_Backend, origin/master)
Type: Feature · Discipline: Engineering · Priority: P0 · Estimate: L · Sprint: HUB-H5 · Lane: CC-B
- [ ] Auth via the existing identity anchor; input `{playerId, partyPlayerIds[]}`
- [ ] `HubPresence` DynamoDB: `PlayerId → {GameSessionId, ShardId, UpdatedAt}` with TTL
- [ ] Placement: friend/party shard with room → `CreatePlayerSession`; else least-empty shard with
      room; else `StartGameSessionPlacement` on `IronicsHubQueue`; returns `{ip, port, playerSessionId}`
- [ ] Hub server writes presence on join/leave via the existing server-SDK path
- [ ] Unit tests for the three placement branches; canary against the dev fleet
```

```
[AFL-3052] Hub fleet + IronicsHubQueue in BagManTentpoleStack   (CDK, own commit)
Type: Pipeline · Discipline: DevOps · Priority: P0 · Estimate: M · Sprint: HUB-H5 · Lane: CC-B + OP
- [ ] Own alias/queue; same OS/instance family as the match fleet (HUB-READ-3); max players =
      `Hub.MaxPlayersPerShard` = 64 (a stack parameter, not a literal); target-tracking scale-to-zero
- [ ] Diff of the stack shows no change to any FlexMatch configuration or match queue
```

```
[AFL-3053] Wire Landing + return-to-hub to the join service
Type: Feature · Discipline: Engineering · Priority: P0 · Estimate: S · Sprint: HUB-H5 · Lane: CC-W
- [ ] `UAFLHubJoinSubsystem` resolver swapped from dev IP to `POST /hub/join`; callers unchanged
- [ ] Friend-follow watched: B joins after A and lands in A's shard
```

```
[AFL-3054] Load measurement — 16 / 32 / 64 simulated clients
Type: Research · Discipline: QA · Priority: P0 · Estimate: L · Sprint: HUB-H5 · Lane: CC-W + OP
- [ ] Headless `-game -nullrhi` clients (or Gauntlet) walking scripted paths across zones
- [ ] Server CPU %, tick ms, per-connection out-bandwidth, replicated-actor counts — table on tracker
- [ ] A solo match completes on the match queue during the soak (isolation proof)
- [ ] Verdict recorded: Replication Graph pulled (AFL-3055) or not, with the numbers that decided it
```

```
[AFL-3055] Replication Graph — conditional on AFL-3054
Type: Feature · Discipline: Engineering · Priority: P1 · Estimate: XL · Sprint: HUB-H5 · Lane: CC-W
- [ ] `UAFLHubReplicationGraph` with `GridSpatialization2D` for hub pawns; always-relevant node for
      volumes/pedestals/mirrors; enabled only for the hub map via `DefaultEngine.ini` per-map driver
- [ ] Re-run AFL-3054; numbers before/after
```

```
[AFL-3056] Linux/Graviton hub fleet — DEFERRED (separate infra programme)
Requires a Linux server cross-compile; not a gate of this programme. Recorded from helper cost model.
```

---

## H6 · CLOSE-OUT

```
[AFL-3060] Tournament / mini-game partition map canary + Assigned Match door
Type: Feature · Discipline: Engineering · Priority: P2 · Estimate: L · Sprint: HUB-H6 · Lane: CC-W + CC-E
- [ ] One partition map reached from a tent, plays, ends, returns (ReturnToHub honoured)
- [ ] Assigned Match: backend supplies a session id; door travels to it; ends → return
```

```
[AFL-3061] Zone prompts, transition beats, ambient audio hooks
Type: Polish · Discipline: Design · Priority: P3 · Estimate: M · Sprint: HUB-H6 · Lane: CC-E
```

```
[AFL-3062] Tracker sync + operator-doc archive
Type: Tech Debt · Discipline: Engineering · Priority: P1 · Estimate: S · Sprint: HUB-H6 · Lane: Claude
- [ ] All H-rows on `BAG_MAN_LIVE_TRACKER.html` with lane + hash
- [ ] `Lobby_Upgrade_Doc.docx`, `Main_Map_Lobby_System_Helper_Doc.docx` archived under `Docs/Hub/`
```

```
[AFL-3063] First-beta walk-through — operator, no cheat, dedicated fleet
Type: Pipeline · Discipline: QA · Priority: P0 · Estimate: M · Sprint: HUB-H6
- [ ] Landing → hub → every zone → deploy → match → return, watched end to end
- [ ] Shipping cook succeeds with the hub map staged
```

---

## TRACK S · SCORE TRACKING AND RANKING — DEFERRED (SSOT §10b)

Opens only after the H2 gate and an explicit operator GO. Listed so the shape is fixed and the
backend is cut once.

```
[AFL-3300] Track S rulings   Type: Research · Lane: OP · Sprint: S0
- [ ] Per loot bonus: reads BestMatchScore or CumulativePoints · period boundaries (default UTC, ISO week) · League scope only
```
```
[AFL-3301] Match-end score event → Leaderboard table   Type: Feature · Lane: CC-B · Estimate: L · Sprint: S1
- [ ] Source = the HMAC-signed match-result Lambda path (P3-TENTPOLE); never a client call
- [ ] Table PK=LB#<Scope>#<Period> (DAY/WEEK/MONTH), SK=Score#PlayerId, top-N GSI; both numbers per player per period; tie = earliest
- [ ] Unit tests: day/week/month bucketing at boundaries; month reset on the 1st
```
```
[AFL-3302] Period close + loot grants   Type: Feature · Lane: CC-B · Estimate: L · Sprint: S2
- [ ] EventBridge cron per period; snapshot top-N; grant via EarnThroughBackend → /earn (the A1.3-proven funnel); idempotent per period+player (conditional write); audit row per grant
```
```
[AFL-3303] Boards   Type: Feature · Lane: CC-E + CD · Estimate: M · Sprint: S3
- [ ] Hub board (DA_AFL_HubDestinations row; Disabled until S), Range board, match-end summary — read-only; brand tokens; from Claude Design handoff
```

## Dependency spine (critical path)

```
AFL-3001 → 3002/3003/3004 → 3005
   → 3010 → 3400 → 3401 → 3402 → 3403 (ruling) → 3404 → 3017     (H1 — the map)
     ∥ 3012 · 3013 · 3014 · 3015 (support code, parallel)
   → 3020 → 3021 → 3022 → 3025 (Range canary) → 3024 → 3026   (H2)
   ‖ 3200 → 3201 → 3210/3211/3212 → 3213 → 3220/3221/3222 → 3223 → 3230 → 3231   (Track C, parallel from H0; C3 gates H3)
   ├→ 3030 → [C1 spine] → 3036 → 3240/3250 → 3038   (H3, opens on 3231)
   ├→ 3041 · 3042 → 3044 → 3045                 (H4, parallel branch; 3043 gated on C2)
   └→ 3051 → 3052 → 3053 → 3054 → [3055] → H5   (H5, parallel branch)
   → 3060 → 3063                                (H6)
```

Lane per ticket (where not stated inline): C++ / config / docs / harness scripts = CC-W; anything
that opens an asset, places an actor, or launches PIE = CC-E; anything in `Bag_Man_Backend` = CC-B.

Estimate roll-up (engineering days, one Claude Code session per branch, operator builds excluded):
H0 ~2 · H1 ~6 · H2 ~6 · Track C0–C3 ~12 · H3 (incl. C4/C5) ~9 · H4 ~9 (+XL if EOS unblocks) · H5 ~7 (+XL if RepGraph) · C6 ~6 · H6 ~4.
H3/H4/H5 run in parallel after H2, so wall-clock is bounded by the longest branch, not the sum.

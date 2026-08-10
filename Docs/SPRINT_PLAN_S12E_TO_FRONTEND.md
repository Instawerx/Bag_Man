# Sprint plan — close S12, stand up the Front End

Written 2026-08-09. Purpose: stop drift. Every item below is either VERIFIED DONE with evidence, or has a
named next action. If work wanders, §7 is the re-entry path.

---

## 1. Where we actually are

**S12 (GameLift `onStartGameSession`) — A/B/C/D/F closed, E in progress.**

| item | state | evidence |
|---|---|---|
| A. Server SDK vendored | ✅ | builds; UE 5.6 confirmed |
| B. Lifecycle adapter | ✅ | `608caf5c` |
| C. The swap | ✅ | was 3 points, not 1 |
| D. AWS fleet/queue/compute | ✅ | Anywhere fleet, compute registered, IaC |
| F. Acceptance test | ✅ | full chain, real currency, `ef180290` |
| E. Player sessions | 🟡 | steps 1–3 done, 4–6 open |

**The economy chain is proven end-to-end over the real GameLift hop.** Match
`B6C77244-4A85-22A3-C4BD-5487CE0E47E7`: escrow 10+10 → 0-7 result → settle pot 20 / rake 1 / payout 19 →
rating −1.94. Ledgers advanced exactly once each.

**S12-E steps 1–3 (this session):**
- Reconnect semantics answered by live probe: **re-accept of an ACTIVE session SUCCEEDS**; **RemovePlayerSession
  is TERMINAL** (`COMPLETED` can never be re-accepted). Session visibility lags activation by **1.44 s**.
- Dropout ruling: **cancelled-refund**.
- Identity gate built and proven: a client claiming another player's id was overridden by GameLift's answer
  and landed on the *true* owner's team.

---

## 2. Outstanding issues ledger

Everything currently loose. Nothing here is allowed to be forgotten.

| # | issue | owner action | blocking? |
|---|---|---|---|
| 1 | `SM_AFL_Railgun` has **NaN mesh distance-field data** | editor: disable Generate Mesh Distance Field, then fix source geometry | player-facing freeze |
| 2 | Engine diagnostic patch in `DistanceFieldObjectManagement.cpp` | remove once #1 fixed; **re-apply after any engine update** | no |
| 3 | **NTFS corruption** — `Intermediate/Build/Win64/LyraServer_CORRUPT_QUARANTINE` undeletable | `chkdsk C: /scan`, then `/spotfix` if errors; user decision (reboot) | build stability |
| 4 | Reporter wiring (payout/rating → `UAFLMatchOutcomeComponent`) uncommitted | commit after the cancel/refund session lands `AFLMatchReporter.cpp` | item 8 below |
| 5 | Scoreboard WBP labels absent | editor: add `CountdownText`, `StakeText`, `PayoutText`, `RatingDeltaText` to `WBP_AFL_MatchScoreboard` | cosmetic only |
| 6 | Gate-predicate fix (`IsSdkReady`, not `IsRosterExternallyOwned`) | built, **NOT yet verified** — test case 3 | local runs |
| 7 | Replay cap + abandonment (`task_3083127f`) | in flight, uncommitted in main tree | item 4 |
| 8 | Soft-path cook guard (`task_9859abae`) | ✅ LANDED — `AFL_COOKED_ASSET` + `AFLCookedAssetRegistry` | no |

### 2.1 ⏸ PARKED 2026-08-09 — the railgun DF NaN

**Status: root cause NOT found. Two attempted fixes failed. Parked by operator decision — it is a
pre-existing content defect and blocks nothing in S12-E or the Front End.**

What is PROVEN:
- The offender is exactly one primitive, every time: `owner='B_WeaponSpawner_C_10' resource='SM_AFL_Railgun'`.
- `B_WeaponSpawner` is **stock Lyra ShooterCore**, and the mesh component is built at runtime from the weapon
  definition — so this is a WEAPON-PICKUP fault, not a railgun-placement fault. Only the railgun appeared
  because only it spawned. Any pickup mesh with the same data problem will do the same.
- The **static mesh's own bounds are VALID**: origin (0.002, 31.498, 1.604), extent (10.515, 42.505, 9.335),
  radius 43.57. The geometry is not obviously broken.
- The **distance-field bounds are NaN**: `dfExtent=(-nan, -nan, -nan)`, and with the second diagnostic pass,
  `matNaN=1` — the LocalToRelativeWorld matrix itself contains NaN, not merely a zero determinant.
- Cost when it fires: `ensureMsgf` → `StackWalkAndDump` **measured at 9.1 s on the render thread**. Presents
  to players as a multi-second freeze with the character locked in pose.
- All 47 meshes under `/Game/BagMan/Equipment` have `generate_mesh_distance_field=False`, while the project
  runs `r.GenerateMeshDistanceFields=True` with Lumen + hardware ray tracing.

What was TRIED AND DID NOT WORK:
1. Disabling *Generate Mesh Distance Field* — it was **already off**. Not the fix.
2. **Enabling** it on `SM_AFL_Railgun` (saved, full re-cook, verified) — NaN persisted, 4,021 `InverseFast`
   errors and 2 ensure stalls. So the DF payload is corrupt **regardless of the flag**, which is the single
   most useful fact here.

TWO CANDIDATE CAUSES, untested:
- **Poisoned DDC entry.** The DF was cached from an earlier bad build and the cook reuses it; toggling the
  flag does not change the DDC key enough to force a genuine rebuild.
- **Nanite fallback.** The asset is Nanite with *Generate Fallback Mesh: Platform Default* and *Distance
  Field Replacement Mesh: None*. DF builds from the fallback, and a degenerate fallback on a 124k-vert
  Tripo-generated mesh would produce exactly this.

THE NEXT TEST, when resumed: change **Distance Field Resolution Scale** on the mesh (e.g. 1.0 → 2.0). That
alters the DDC key and forces a real rebuild. If the NaN survives a genuine rebuild it is the geometry or the
Nanite fallback, and the mesh needs re-importing or repairing in Blender.

HOW TO SEE IT AGAIN: the engine has no way to name the offending primitive on its own. The diagnostic patch
(logging `PrimitiveSceneProxy->GetOwnerName()` / `GetResourceName()` beside the `InverseFast` call in
`DistanceFieldObjectManagement.cpp`) is REVERTED and must be re-applied to identify it. Do not revert it
again before a fix is confirmed — that mistake cost a rebuild cycle here.

---

### 2.2 The railgun evidence, in detail

```
BAGMAN_DF_NAN: owner='B_WeaponSpawner_C_10' resource='SM_AFL_Railgun'
det=-nan axisLen=(nan,nan,nan) localBoundsExtent=(-nan,-nan,-nan) localToVolumeScale=-nan
```

`localBoundsExtent` is `DistanceFieldVolumeData::LocalSpaceMeshBounds`, read from the asset — **already NaN
before any transform maths**. So this is baked-in asset corruption, not a bad actor placement. That is why a
sweep of all 361 primitives' component scales found nothing.

Cost when it fires: `ensureMsgf` → `StackWalkAndDump` **measured at 9.1 s on the render thread**. One bad
asset stalls the whole renderer for ~9 s, which players read as a T-pose freeze.

Only ONE asset is affected. Asset: `Content/BagMan/Equipment/Railgun/SM_AFL_Railgun.uasset`.

---

## 3. SPRINT 1 — make the game playable and close S12-E (highest value first)

### 1.1 Fix the railgun (unblocks the freeze)
Editor. Disable *Generate Mesh Distance Field* on `SM_AFL_Railgun`, re-cook, confirm `BAGMAN_DF_NAN` and
`InverseFast` are both **absent** from a cooked client log. Then remove the engine patch (#2).
Follow-up: the mesh is generated content — inspect for NaN/degenerate verts and regenerate DF properly.

### 1.2 Verify test case 3 (the regression guard)
Launch a local dedicated server with `?MatchmakerData=` and **no** `?PlayerSessionId=`. A client must connect
normally. This is the exact case the first gate predicate broke, and the fix is unverified.

### 1.3 S12-E step 4 — lock the session at match start
`UpdatePlayerSessionCreationPolicy(DENY_ALL)` when the match starts; reopen only if a reconnect window is
active. Prevents late injection into a settled, staked roster.

### 1.4 S12-E step 5 — disconnect / reconnect protocol
Ruling: **cancelled-refund**. Constraints now known, not assumed:
- **NEVER** call `RemovePlayerSession` on a dropout — it is terminal and permanently bars the player from a
  match their stake is already escrowed in.
- Reconnect needs no new backend: re-accepting an ACTIVE session succeeds.
- A dropout that never returns is a **third** `EAFLMatchCancelReason`, alongside `Abandoned` / `ReplayCap` —
  coordinate with `task_3083127f`, do not build a parallel mechanism.

### 1.5 S12-E step 6 — run the 9-case matrix
Cases 1, 2, 6 ✅ passed. Remaining: 3 (local, no session), 4 (forged id), 5 (reused id), 7 (reconnect inside
window), 8 (beyond window), 9 (join after `DENY_ALL`).

**Exit criteria for Sprint 1:** S12 fully closed; no render stall; matrix green.

---

## 4. THE BRIDGE — why Front End is now on the critical path

S12-E changed the client contract. **A client must present `?PlayerSessionId=` to join a GameLift match.**
Today only the test harness supplies it — I paste it in by hand from the allocator response.

Nothing in the shipping client can currently join a real match.

So the Front End is no longer cosmetic polish; it is the thing that makes S12 usable:

```
PLAY  ->  request a match (backend)      ->  allocator creates the placement
      ->  receive ipAddress:port + playerSessionId
      ->  travel to  <ip>:<port>?PlayerSessionId=psess-...
```

That flow is the minimum for a shippable client, and it is Sprint 2.

---

## 5. SPRINT 2 — Front End: PLAY is the only thing that matters first

**What already exists** (verified, not assumed): `L_LyraFrontEnd`, `B_LyraFrontendStateComponent`,
`W_FrontEndHUDLayout`, and C++ for `AFLW_FrontEndMarket`, `AFLW_LoadoutBase`, `AFLW_LoadoutTileBase`,
`AFLLoadoutDisplayPawn`, `AFLLoadoutPod`, `AFLCosmeticLoadoutComponent`. The observed menu is
PLAY / SETTINGS / LOADOUT / STORE / IDENTITY (coming soon) / SHOW REPLAY / QUIT.

### 2.1 PLAY → a real match (THE gate for everything)
- Client-side call to a matchmaking/allocate endpoint
- Receive `{ipAddress, port, playerSessionId}` — the allocator already returns exactly this shape
- `ClientTravel` to `<ip>:<port>?PlayerSessionId=...`
- Failure UX: placement failed, timed out, session rejected

### 2.2 Identity / login
The observed front end reads **"Player 2: Not logged in"**. Escrow debits a PlayFab account, so *who is
logged in* must be real before PLAY can stake anything. The PlayFab↔EOS login already ships; the front end
must surface and require it.

### 2.3 Return path (already works — protect it)
`CONTINUE`/countdown → `ReturnToMainMenu()` → frontend. Proven this session. Add it to a regression list so a
future change cannot silently remove the only way out of a finished match (it already happened once).

### 2.4 LOADOUT / STORE
C++ exists. Scope after 2.1/2.2 — they are meaningless until a player can log in and play.

**Exit criteria for Sprint 2:** a player launches the shipping client, logs in, presses PLAY, lands in a
staked GameLift match, plays, sees results, and returns to the lobby — with no hand-pasted ids anywhere.

---

## 6. SPRINT 3 — hardening (after the loop closes)

- Remove the engine diagnostic patch; add a permanent asset-validation check for NaN DF data at cook time
  (the soft-path guard pattern applied to a second class of silent content fault)
- Replay cap + abandonment lands and is verified end-to-end against `cancelled-refund`
- Rating/settle atomicity: they are independent calls today, and rating applied to a refunded match earlier
  in this session. Rank moving without a matching settlement is a real inconsistency.
- Escrow rows never leave `escrowed` even after a successful settle — the escrow table alone cannot answer
  "what is outstanding". Either flip the row on settle, or document the join as the only correct query.

---

## 7. RE-ENTRY PATH — if we drift, start here

1. Read §2's ledger. Anything unticked is still owed.
2. Ask: *does this advance Sprint 1's exit criteria?* If not, it is Sprint 3 or it is noise.
3. Verified-done requires **evidence in a log or a ledger row**, never "it should work".
4. Content and cook faults do not reproduce in PIE. A cooked run is the only proof.
5. A build failure is not automatically code — this session lost hours to NTFS corruption presenting as
   compiler and UHT errors. Check the filesystem before rewriting source.

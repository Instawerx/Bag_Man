# IRONICS_LOBBY_HUB_SSOT

**Status:** NEW. Architecture and scope definition for the Super Lobby ("Outpost Earth MWR") — the
walkable social entrance to every existing IRONICS system.
**Date:** 2026-08-26
**Basis:** `Lobby_Upgrade_Doc.docx` (operator intent, authoritative) · `Main_Map_Lobby_System_Helper_Doc.docx`
(operator-supplied technical spec) · `BAG_MAN_LIVE_TRACKER.html` P-HUB card (prior rulings) ·
`IRONICS_CHARACTER_CREATOR_SSOT.md` · `IRONICS_PRICING_SSOT.md` · shipped economy/matchmaking state.
**Rule:** every mechanism claim cites a file and line or is marked **[VERIFY]** — resolved by HUB-READ
passes before any authoring. No asset name or path below is invented; proposed names are marked
**(proposed)** and are confirmed or replaced at HUB-READ.
**Governing document:** `IRONICS_LOBBY_UX_FLOW_SSOT.md` — the operator's flow diagram transcribed; this SSOT
implements its nodes and edges and is subordinate to it.
**Companions:** `IRONICS_LOBBY_HUB_ROADMAP.html` (phase order + gates) · `IRONICS_LOBBY_HUB_TASKS.md`
(tickets) · `IRONICS_LOBBY_HUB_CLAUDE_CODE_BRIEF.md` (lane operating protocol) ·
`IRONICS_CC_INTEGRATION_PLAN.md` (Track C — the creator, its intent lock, and the shared preview
spine + widget kit that the PX, Barracks and Labs zones all open).

---

## 0 · What this programme is, and what it is not

**It is an entrance.** The hub adds a walkable, social, AAA front door in front of systems that already
exist and are proven: FlexMatch matchmaking (all 30 cells routed), the League/Staked doors, the wallet →
`IAFLCosmeticPersistence` → PlayFab purchase seam (Phase 2 canary closed on `AFL.Facemask.Flag.Japan`),
the `#43` cosmetic selection seam, the Character Creator programme, the loadout system.

**It is not a rewrite of any of them.** Every hub zone is a *front-end over an existing backend*
(P-HUB card, live tracker: "Zones = front-ends over SEPARATE backends"). The hub calls the proven
RPC, widget, or travel path; it never re-implements one.

**Operator intent is fixed.** The zone list, the flows, the map choice, and the store/loadout display
model in `Lobby_Upgrade_Doc.docx` are not design inputs to be revisited. Where the helper doc's
mechanism would break a proven system, §2 records a *conformance ruling* — same behaviour, delivered
through the existing seam — and cites why.

**Lane model (amended 2026-08-26):** Claude Code runs everything it can — code and backend in git
worktrees (parallel), asset/placement/PIE-harness work through its direct editor connection (one
session at a time, one editor). AIK is used only where the editor connection lacks a required tool,
as established by the H0 capability probe. See `IRONICS_LOBBY_HUB_CLAUDE_CODE_BRIEF.md` §1–§4.

**Two things must never regress:** the match loop (fire, damage, dismember, energy, extraction,
respawn) and the matchmaking/lobby pipeline. §9 is the guardrail set; every phase gate re-proves both.

---

## 1 · The place

**Map:** Military Mega Base Pack — demo map, imported, migrated into `AFLHub`, sanitised, renamed, its
buildings assigned to our zones by operator ruling. Full build order: `IRONICS_HUB_MAP_BUILD_SPEC.md`.
**(proposed)**
`L_AFL_OutpostEarth` under the `AFLHub` GameFeature content root (new content goes in a GameFeature,
never `/Game`). Player-facing name: **Outpost Earth MWR**. **[VERIFY at HUB-READ-1: is the pack on
disk, where, and what is its demo map name?]** If absent, the operator imports it from Fab before H1.

**One continuous space.** The player walks between zones. Within the hub there is never a load
(level streaming / World Partition by proximity — SEAM RULE, P-HUB card). Leaving the hub for a
match, a tournament map, or a mini-map is a Lyra Experience travel with a transition beat — that is
correct and expected.

### 1.1 Zone table (from `Lobby_Upgrade_Doc.docx`)

| Zone | Player promise | Backend it fronts | Enter → |
|---|---|---|---|
| **Main Zone** (Outpost Earth) | Walk, chat, see players + friends, wear accessories, carry weapons, **no fire** | Hub dedicated server, presence | — |
| **Shooting Range** | Fire authorised — **a separate map**; the door transports you there and back | Own experience (no fire restriction), existing weapon abilities, existing target dummies; range scoring later (Track S) | Door → ExperienceTravel → Range map → ends/exit → return to hub |
| **PX Store** | Every asset displayed and buyable — try-on / hold, mirrors, jewellery counter, weapon displays, masks, stickers, robots | Catalog `DA_AFL_CosmeticCatalog`, wallet `ClientRequestPurchase`, product-page UI (in flight) | Pedestal interact → product page |
| **Robo Labs** (Lab = CC) | Build a robot | Character Creator — Track C creator shell (`IRONICS_CC_INTEGRATION_PLAN.md`) | Door → creator shell; `Disabled` until C3 green |
| **Loadout Barracks** | See and equip owned weapons/assets, displayed like the store | `AFLW_LoadoutBase` path, `ServerSetCosmeticSelection` | Pedestal interact → equip |
| **Patriot Café** (Lounge 1) | Social club — friends, private room | EOS lobby (sub-lobby) | Door → club join/create |
| **EM Lounge** (Lounge 2) | Social club, second flavour | EOS lobby (sub-lobby) | Door → club join/create |
| **Deployments** | Go play — Staked Lobby or League Lobby | FlexMatch via the existing League/Staked door widgets | Volume → existing matchmaking UI |
| **Tournaments / Challenges** | Tent → tournament or mini-map, plays, **ends**, returns | Experience travel to a partition/mini map | Tent volume → travel (destinations data-driven) |
| **Tournament Mini Games** | Tent → mini-game surface | Experience travel | Same mechanism |
| **Assigned Match** | Enter an assigned match; it **ends**; return to hub | Existing match servers | Same mechanism |
| **Landing Page** | Sign in / Sign up; 3D map-shot backdrop; new player → wallet with free starters; return player → last set character | IRONICS accounts / EOS OAuth; `GrantedFree` auto-ownership (`AFLWalletComponent.cpp:147-171`); persistence read-back | Enter Base → hub join |

**The "ends → return to hub" invariant (flow §1.3).** Every destination that leaves the hub returns to
the hub when it ends — the two loops on the diagram (E6→N5→E7, E9→E10→E11). The hub is the home
state. Nothing returns to a menu; the Landing Page is entered once per boot.

---

## 2 · Conformance rulings (engineering, not redesign)

The helper doc was written without the shipped codebase in front of it. These rulings keep its
architecture and player behaviour, delivered through what is already proven. Each is an engineering
choice on the same intent, not a change to the intent.

### 2.1 Purchase goes through the existing wallet seam, not a client-direct PlayFab call

Helper §4.2 has the client call `PlayFabClientAPI::PurchaseItem` then send `Server_EquipPermanentCosmetic`.
On disk the purchase already runs server-side through `UAFLWalletComponent::ClientRequestPurchase`
(`AFLWalletComponent.cpp:376-409`) → the `IAFLCosmeticPersistence` seam → PlayFab, with catalog-price
enforcement (`PurchaseItem` rejects `WrongPrice` unless seed VO equals the row's `PriceVolts`) and the
anti-spoof ladder A1.3/A1.4 closed. A second, client-direct path would be a second thing to secure
and would bypass the ladder.

**Ruling:** the PX Store product page calls `ClientRequestPurchase` — the same call site the
front-end market uses (`AFLW_FrontEndMarket.cpp:476,483,904,906`). Equip after purchase goes through
`ServerSetCosmeticSelection` on the `#43` seam. The player experience (preview locally → buy → it is on
you, visible to everyone nearby) is identical to the helper's description.

**Consequence:** helper doc §2 (PlayFab → GameLift webhook) is **not required for v1**. The hub's own
dedicated server *is* the server that processes the purchase, so there is nothing to push to it. The
"on map join, read persisted cosmetics" recovery (helper §5) already exists: spawn reads the persisted
selection. The webhook is recorded as a deferred option only if the store is ever hosted off the hub
server.

### 2.2 The hub pawn is the Lyra hero pawn with a hub network profile, not a new `AHubCharacter`

Helper §2.1 proposes `AHubCharacter : ACharacter` with movement replication bypassed. Two disk facts
make a separate pawn class the wrong tool:

- **The preview is the product** (CC SSOT §5.3). Every cosmetic path — robot body via
  `UAFLCharacterPartSelectorComponent`, facemask via `RefreshFacemaskForPawn`, skin colour via the
  `M_AFL_Character` MID push, accessories via the proven `AddCharacterPart` mechanism, carried weapons
  via Lyra Equipment — resolves onto the Lyra hero pawn. A second pawn class would have to
  re-implement all of it or render a stand-in, which the CC SSOT calls bait-and-switch.
- **P-CONTROLS doctrine:** CMC mechanics attach as a GameFeature `UActorComponent`, never a CMC
  subclass or pawn reparent. `UAFLCharacterMovementComponent` + `AAFLCharacter` are already dead code.

**Ruling:** hub experience uses the existing hero `ULyraPawnData` **[VERIFY exact asset]** and attaches
**(proposed)** `UAFLHubNetProfileComponent` via `GameFeatureAction_AddComponents` in the hub experience
only. The component delivers the helper's bandwidth targets with engine facilities:

| Helper §2 target | Delivered by |
|---|---|
| 10–15 Hz positioning | `NetUpdateFrequency = 15`, `MinNetUpdateFrequency = 5` on the pawn |
| Quantised position + 1-byte yaw (`FHubLowFrequencyTransform`) | `ReplicatedMovement.LocationQuantizationLevel = RoundOneDecimal`, `RotationQuantizationLevel = ByteComponents`, `VelocityQuantizationLevel = RoundWholeNumber` — engine `FRepMovement`, no custom net struct |
| Strict `NetCullDistanceSquared`; PX Store never sees Deployment Zone | Per-zone cull distance set by the component from the pawn's current `Hub.Zone.*` tag |
| Stripped tick | Hub ability set excludes combat-only components; no lag-comp, no energy, no extraction in the hub experience |

A custom `USTRUCT` with a net serializer would have to live in `AFLNetTypes` (doctrine: every
net-serialised struct lives in an always-loaded module). The engine quantisation fields give the same
bytes-on-wire without a new struct.

**Scale path (fix-to-diagnose, not assumption):** cull + frequency first, measured under simulated
load in H5. Replication Graph with a 2D spatial grid (`UReplicationGraphNode_GridSpatialization2D`)
is the next rung and is only pulled when the measurement says so.

### 2.3 Fire safety is fail-closed on one additive tag; the Range is a separate map

Helper §3 adds `Capability.Weapon.CanFire` on range overlap and checks it in the fire cycle. On disk,
fire is a Lyra GAS ability; the equivalent check is the ability's activation tags. And by operator
ruling (2026-08-26) **the Shooting Range is its own map**, entered and left by the door mechanism —
not a zone inside the hub.

**Ruling (fail-closed):** one tag `Hub.Restriction.NoFire` **(proposed)** is added to
`ActivationBlockedTags` on every AFL fire ability (Pulse, Beam, hand cannons — **[VERIFY the list at
HUB-READ-2]**). The **hub experience** applies an infinite GE granting that tag on possession. The
**Range experience** simply never applies it — authorisation is *by experience*, which is the
helper's `Capability.Weapon.CanFire` state expressed as "which experience you are in". No volume
removes the tag anywhere; there is no in-hub authorised-fire zone. Default is no fire; a client spoof
resolves to "cannot fire". In a match or on the range the tag never exists, so fire there is untouched
by construction.

`AAFLHubZoneVolume` keeps its zone-tag role (cull profile, prompts, chat channel — §4.1) and loses
`bAuthorisesFire`.

**Proven-Sibling Rule applies:** the blocked-tag addition is one line per ability; the diff is
reviewed against the proven Pulse ability and each file cited.

### 2.4 One hub instance is a shard; capacity is a data value

Helper §1 targets 1,000 players in one instance. The beta target on record is 1,000 *registered*
testers with a 36-concurrent organic peak. One 1,000-player instance is the highest-risk item in the
helper doc (a single failure domain, unmeasured CPU under Lyra's pawn stack) and no part of the player
promise depends on it — players need to see *the players and friends around them*, not all of them.

**Ruling:** the hub runs as GameLift game sessions ("shards") of `Hub.MaxPlayersPerShard` players — a
fleet/queue data value, starting at **64** and raised on measurement. The hub join service
(§6) places a player into the shard holding their friends/party when one has room, otherwise the
least-empty shard, otherwise a new one. This is a scale strategy on the same architecture; if H5
measurement shows one instance comfortably carries far more, the number goes up. The player never
sees a shard.

**Operator confirmation requested (one line):** shard model as above, start at 64. See §11.

### 2.5 Store display reuses the map weapon-spawner system, recoloured — as ruled

`Lobby_Upgrade_Doc.docx`: "Use map weapon spawner systems recolored for our store gives us all the
functionality we need and AAA Display, same for Loadouts." This matches the D1 ruling already on
the tracker (3D showroom: data-driven pedestals spawned from catalog entries, never hand-placed;
walk up → preview → equip/purchase through the same server-authoritative paths).

**Ruling:** **(proposed)** `AAFLDisplayPedestal` is a child of the Lyra weapon spawner class
**[VERIFY: `ALyraWeaponSpawner` in `Plugins/GameFeatures/ShooterCore` — confirm class and path]**, with
grant-on-overlap replaced by interact-to-inspect, the spinning display mesh driven from the catalog
row's display asset, and the pad material re-instanced in the brand palette (Electric Neon Blue
`#1E5AFF`; Watts magenta `#FF00D5` reserved for staked/premium). Racks are data: **(proposed)**
`AAFLDisplayRack` holds a catalog filter (type/axis/collection) and N spawn transforms; at BeginPlay it
spawns one pedestal per matching row. New catalog row → new pedestal, no map edit. Barracks uses the
same actor with the filter "owned by local player".

### 2.6 Social clubs are EOS lobbies, gated on the EOS user login that is already blocked

Helper §6 maps clubs to `IOnlineLobby::CreateLobby` on EOS with `FriendsOnly` / `InviteOnly` and a local
visibility mask. That is the right tool. On disk: EOS-AUTH-C1 (Connect) is proven; **EOS-AUTH-C2
(Epic Account user login) was blocked on Epic Application verification, OAuth 1012**. Friends,
presence, invites, and lobbies all require the EAS user identity.

**Ruling:** social-club work (H4) carries a hard entry gate — C2 green on disk. **[VERIFY C2 status
at HUB-READ-3.]** Everything in H1–H3 is built so that club membership is one more replicated
`FName ClubId` on the hub player state; the visibility mask reads that value. If C2 stays blocked, a
hub-server-authoritative club (no EOS) is the documented fallback and needs no rework upstream.

### 2.7 Deployment gateway calls the existing door widgets; party tickets use FlexMatch multi-player tickets

Helper §7: walking into Deployments pushes the existing matchmaking overlay; on confirm the client
disconnects from the hub and enters the queue. **Ruling:** exactly that, calling the existing
League/Staked door surface (`IRONICS_LEAGUE_DOOR_SPEC.md` — **[VERIFY widget class and open cheat at
HUB-READ-2]**). No new matchmaking code path; no FlexMatch rule set, queue, or SSM routing edit.

Party queue: FlexMatch accepts one ticket carrying several players. The leader submits the ticket with
all party `PlayerId`s (existing backend, one additive field — **[VERIFY the start-matchmaking Lambda's
request shape]**); the ticket id is written to the EOS lobby attributes (helper §7.1) so members follow.
Match found → every member travels to the match server with their own player session, as solo does
today. This is the helper's flow exactly; it only uses FlexMatch's native group ticket instead of
per-member registration.

---

## 3 · Module and experience shape

```
Plugins/GameFeatures/AFLHub/                       (proposed, new GameFeature)
├── AFLHub.uplugin
├── Source/AFLHub/
│   ├── Public/
│   │   ├── AFLHubNetProfileComponent.h            §2.2
│   │   ├── AFLHubZoneVolume.h                     §2.3, §4.1
│   │   ├── AFLHubDestinationVolume.h              §4.2
│   │   ├── AFLDisplayPedestal.h / AFLDisplayRack.h §2.5, §5
│   │   ├── AFLHubPreviewAnchor.h                  §5.2
│   │   ├── AFLHubMirror.h                         §5.3
│   │   ├── AFLCosmeticPreviewComponent.h          §5.2 (client-local try-on)
│   │   ├── AFLHubChatComponent.h                  §7.1
│   │   └── AFLHubClubComponent.h                  §7.2
│   └── Private/…
└── Content/
    ├── Maps/L_AFL_OutpostEarth                    (renamed pack demo map)
    ├── Experiences/B_AFL_Experience_Hub           (ULyraExperienceDefinition)
    ├── Data/DA_AFL_HubDestinations               §4.2
    ├── Data/DA_AFL_HubZoneProfiles                §2.2 cull table
    ├── Abilities/DA_AFL_AbilitySet_Hub
    └── UI/…                                       zone prompts, nameplates
```

- **Experience:** `B_AFL_Experience_Hub` composes: hero pawn data **[VERIFY]**, `AddComponents`
  (`UAFLHubNetProfileComponent`, chat, club, preview) to the pawn / player state, the hub ability set
  (movement, interact, dash; no combat-only sets), the hub HUD layout (nameplates, zone prompt, chat),
  and the `Hub.Restriction.NoFire` GE on possession. The match experiences are not opened in this
  programme.
- **Dependencies:** `AFLHub` depends on `LyraGame`, the AFL cosmetic modules, `AFLNetTypes`,
  `AFLVFX` (root plugin). It never introduces a dependency *into* a match GameFeature.
- **Server target:** `LyraServer` (`DefaultServerTarget=LyraServer` already durable in
  `DefaultEngine.ini`). The hub is a dedicated-server map in the same shipping cook.

---

## 4 · Zones and doors

### 4.1 `AAFLHubZoneVolume` (proposed)

Server-authoritative trigger. On overlap begin/end it applies/removes a per-zone infinite GE that
grants `Hub.Zone.<Name>`. It never touches `Hub.Restriction.NoFire` (§2.3).
The net-profile component listens for zone tag changes to swap cull distance. Client-side, the same
tag drives the zone prompt widget and (later) chat channel selection. Tags declared in the `AFLHub`
tag ini, never in the combat tag ini.

### 4.2 `AAFLHubDestinationVolume` + `DA_AFL_HubDestinations` (proposed)

A door is a volume with a `DestinationId`. The data asset maps id → action:

| Action | Payload | Used by |
|---|---|---|
| `PushWidget` | activatable widget class, layer (`UI.Layer.GameMenu`), optional pre-focus SKU | PX product page, Barracks, Robo Labs (CC shell), Deployments (League/Staked door) |
| `ExperienceTravel` | map + experience id, return-to-hub policy | **Shooting Range (the first, canary)**, Tournaments, Mini Games, Assigned Match |
| `JoinClub` | club flavour (Café / EM), privacy default | Lounges |
| `Disabled` | "Coming soon" prompt text | Any door whose backend is not yet proven |

Widget push uses the `afl.Store.Open` pattern — `UCommonUIExtensions::PushContentToLayer_ForPlayer`
(live tracker, store slice 1). Travel goes through the Lyra experience travel path the match already
uses. **A door with no proven backend ships as `Disabled`, never as a stub that pretends.**

### 4.3 Return-to-hub

Any experience reached from a door carries `ReturnToHub = true` in its travel options. On match /
tournament end, the existing return path **[VERIFY: what the client does today at match end]** is
redirected to a hub rejoin (§6) instead of the front-end menu. The Landing Page remains the cold-boot
entry only.

---

## 5 · PX Store and Barracks — the spatial retail layer

The retail layer is **client-local until the moment of commit** (helper §1 asymmetric topology):
0% server cost for browsing, previewing, spinning, mirrors. The server sees one RPC — the purchase
or the equip — through the existing seams.

### 5.1 Pedestal → product page

Interact on a pedestal (existing interact ability + `UAFLGrabbableComponent`-style seam — **[VERIFY the
interact verb's interface at HUB-READ-2]**) → camera blends to the pedestal's `AAFLHubPreviewAnchor`
(`SetViewTargetWithBlend`, cubic, 0.4 s) → the product page (the in-flight product-page redesign,
three-lane design pipeline) is pushed pre-focused on that SKU. Buy → `ClientRequestPurchase`. Back →
camera blends home, page pops. The pedestal does not know about money; the page does.

### 5.2 Try-on / hold — `UAFLCosmeticPreviewComponent` (proposed, client-only) — **owned by Track C1**

Applies a candidate cosmetic to the **local pawn only** using the same apply functions the real
selection uses (facemask apply, MID push, `AddCharacterPart` for accessories/robots, weapon mesh
socket for "hold"), with no RPC, and restores the authoritative selection on exit or on any
selection replication. Because it calls the same functions the `#43` path calls, what the player
sees at the mirror is what they get. Exiting the PX zone volume force-restores. Nothing in this
component is replicated; the server never knows a preview happened.

### 5.3 Mirrors — `AAFLHubMirror` (proposed)

`USceneCaptureComponent2D` with `bCaptureEveryFrame = false` by default. A front box trigger — **local
overlap only** (`IsLocallyControlled`) — enables capture, sets `PrimitiveRenderMode = UseShowOnlyList`
with the local pawn and its part actors, and disables on exit. Capture resolution and rate are
`DA_AFL_HubZoneProfiles` values. Exactly helper §5.

### 5.4 Racks are catalog-driven

`AAFLDisplayRack` filter examples: `Type == Weapon` (weapons wall), `Axis == Facemask` (mask wall),
`Type == Accessory` (jewellery counter), `Type == Character` (robots), sticker packs when the sticker
axis lands (CC-7 — blocked on UV work; the rack shows `Disabled` until then). Barracks: filter
`OwnedByLocalPlayer`. The rack reads `UAFLCosmeticCatalogSubsystem` **[VERIFY name]**; no second
catalog.

### 5.5 Replace-after-proven

The front-end store and loadout screens stay live until the spatial layer proves the identical loop
(H3 gate: buy the `Flag.Japan` canary at a pedestal, see it on both clients, respawn-durable). Only
then are the old entry points removed — and only the entry points; the widgets they open are the
same widgets the hub opens.

---

## 6 · Hub join service (backend — `Bag_Man_Backend`, separate commits)

New Lambda + route **(proposed)** `POST /hub/join` in `BagManTentpoleStack`:

1. Authenticate (existing identity anchor).
2. Resolve the caller's party/club members' current shard (DynamoDB `HubPresence` table:
   `PlayerId → {GameSessionId, ShardId, UpdatedAt}`; TTL-expired rows are ignored).
3. If a friend shard has room → `CreatePlayerSession` there. Else least-empty shard with room.
   Else `StartGameSessionPlacement` on **(proposed)** `IronicsHubQueue` — a queue and fleet alias
   **separate** from the match queues so hub load can never starve match capacity, and no FlexMatch
   configuration is touched.
4. Return `{ip, port, playerSessionId}`; the client travels.

The hub server writes presence on join/leave (existing server SDK path). Landing Page and
return-to-hub both call this one route. `Hub.MaxPlayersPerShard` is the fleet's max-player value,
data not code.

---

## 7 · Social layer

### 7.1 Text chat (hub server relayed)

EOS provides voice, presence, friends, and lobbies; it does not provide text chat. **Ruling:** hub
text chat is a server-relayed channel on **(proposed)** `UAFLHubChatComponent` (player state): client
RPC → server validates rate + length + profanity (same filter the CC build-name rule needs, CC-5.4 —
build once) → multicast to recipients by channel: *Proximity* (within the sender's zone / radius),
*Club* (same `ClubId`), *Party*. Relevancy already limits fan-out. Voice: EOS RTC once C2 is green
(H4).

### 7.2 Clubs — `UAFLHubClubComponent`

Replicated `FName ClubId` + privacy. H4 binds it to an EOS lobby id; until then it is set by a
server-authoritative "create/join club" action from the lounge door. The **visibility mask** is
local: pawns whose `ClubId` differs from the local player's active club (when the local player is
inside a lounge with an active club) are hidden via the existing part-actor visibility path — never
destroyed, never un-replicated (helper §6.1: private instance without a map load).

### 7.3 Friends and presence

Nameplates (hub HUD) show display name and a friend marker. Friend list from EOS friends (C2) —
until then, party-only. The Landing Page "load last set character" is the existing persistence
read-back; "load wallet with free starters" is `GrantedFree` auto-ownership on first wallet read.

---

## 8 · Map preparation (helper §8, sequenced)

| Step | Phase | Lane |
|---|---|---|
| Delete demo AI BPs, ticking targets, sequences, physics props | H1 (blocks zone placement) | CC (editor) |
| Rename map, set hub experience as the map's default | H1 | CC (editor) |
| Place zone volumes (10 in-hub zones) + destination volumes + PlayerStarts | H1–H2 | CC (editor) |
| Range map **(proposed)** `L_AFL_ShootingRange` in `AFLHub`: own experience (no NoFire GE, full loadout), existing target dummies, exit door back to hub | H2 (the travel canary) | CC (editor) |
| HISM merge of prop clusters (Merge Actors) | H5 | CC (editor) — AIK only if the connection cannot drive Merge Actors |
| NavMesh bounds shrunk to walkable surfaces | H5 | CC (editor) |
| Retail shelving on a dedicated trace channel **(proposed)** `ECC_HubRetail` for client-only tracing | H3 | CC worktree (channel config) → CC editor (assignment) |
| Lighting/streaming budget pass, World Partition cell sizing | H5 | CC (editor) + operator PIE |

---

## 9 · Regression guardrails (every phase gate re-runs these)

1. **No edits to any match experience asset, FlexMatch rule set, queue, SSM routing parameter, or
   League/Staked door code path** in this programme. The hub *calls* the door; it does not touch it.
2. **`FAFLCosmeticSelection` keeps its shape and read site.** Try-on is client-local and never writes it.
3. **One purchase path.** Any hub code calling PlayFab directly is a defect.
4. **Fire abilities change by exactly one `ActivationBlockedTags` entry each.** Match PIE fire proof
   (2-client, live Pulse hit → damage) runs at every gate from H1 on.
5. **Hub code lives in `AFLHub`.** No hub symbol in a match GameFeature; no match-only component in the
   hub experience.
6. **Code and content never share a commit; product and instrument never share a commit.** Backend
   commits go to `Bag_Man_Backend` `origin/master` — never folded into a game-repo commit.
7. **Clean tree before any D: (server/cook) session.** The owed clean-tree triage is H0's first item.
8. **Proof standard:** ✅ = watched in PIE on a controllable pawn; two clients wherever replication is
   claimed; dedicated server wherever travel or join is claimed. Never "compiles".
9. **Every door with an unproven backend ships `Disabled`.** No stub that pretends.
10. **Canary before scaling:** one zone, one pedestal, one destination, one shard — each proven
    end-to-end before the pattern is applied across the set.

---

## 10 · Cost (helper doc figures, carried)

The helper's $130–480/month at 1,000 CCU on one c6g/c7g.4xlarge assumes Linux/Graviton and a
Spot-first queue. **[VERIFY at HUB-READ-3: current match fleet OS and instance family.]** A Linux
server cross-compile is a build-pipeline change and is **not** in this programme; the hub fleet
launches on whatever the match fleet already runs, and the Linux/Graviton move is recorded as a
separate infra ticket (H5 optional). Scale-to-zero via target-tracking applies to the hub queue from
day one.

---

## 10b · Track S — score tracking and ranking (deferred; designed now so it isn't throwaway)

**Operator ruling (2026-08-26):** League play gets score tracking and ranking with **loot bonuses**;
leaderboards for **Top score of Day, Week, Month**; **fresh start on the 1st of every month**; best
competitive-game practice. **Not a priority until the main map / lobby system is working** — Track S
does not start before the H2 gate and an explicit operator GO. What is fixed now is only the shape,
so H5's backend and the hub's surfaces don't have to be re-cut later.

Shape (best practice, cross-linked to the P-SCORING pillar and S22 AFL-2201..2206 on the tracker):

1. **Server-authoritative score events only.** Scores derive from the dedicated server's match-end
   report through the existing HMAC-signed result path (P3-TENTPOLE `GetMatchResult` → Lambda). A
   client never submits a score. Anti-cheat = the same G4 rule scoring already carries.
2. **Period keys, not mutable counters.** Backend table **(proposed)** `Leaderboard`:
   `PK = LB#<Scope>#<Period>` (`DAY#2026-08-27`, `WEEK#2026-W35`, `MONTH#2026-08`), `SK = Score#PlayerId`,
   with a GSI for top-N reads. Day/Week windows nest inside the Month; the Month is the reset unit.
   Boundaries in UTC unless ruled otherwise; week = ISO, Monday 00:00 UTC.
3. **Two numbers per player per period, stored side by side:** `BestMatchScore` (a single match's
   score — "top score") and `CumulativePoints` (sum across the period — "ranking"). Which one a loot
   bonus reads is a data rule per bonus, not code. Tie-break: earliest achieved wins.
4. **Bonuses granted at period close, server-side.** An EventBridge-scheduled Lambda closes each
   period, snapshots the top-N, and grants loot through the **existing earn seam** (`EarnThroughBackend`
   → `/earn`) — the same funnel A1.3 proved, so grants are authoritative and auditable. Idempotent per
   period+player (a re-run never double-grants).
5. **Read surfaces:** a hub leaderboard board (Main Zone or Deployments — data row, `Disabled` until
   Track S), the Range map's own board, and the match-end summary. All read-only views of the table.
6. **League only.** Scope key carries the cell/tier so Staked and League never share a board unless
   ruled.

Track S phases (when opened): S0 ruling on cumulative-vs-best per bonus + timezone · S1 match-end
score event → table (CC-B) · S2 period-close Lambda + grants through the earn seam (CC-B) · S3 hub +
range boards + summary (CC-E/CD). Proof: two dev accounts, two matches, boards correct on both
clients; force a period close; the bonus lands in the wallet through the seam; re-run is a no-op.

---

## 11 · Decisions needed from the operator

Only genuine product-intent items. Everything else is ruled from doctrine on disk.

| # | Decision | Default if silent |
|---|---|---|
| 1 | Shard model (§2.4): hub as N-player GameLift sessions with friend-follow, starting at 64 | Proceed with 64 |
| 2 | Map asset name `L_AFL_OutpostEarth` / experience `B_AFL_Experience_Hub` | Proceed |
| 3 | Chat: text via hub server relay; voice via EOS after C2 (§7.1) | Proceed |
| 4 | Club fallback if C2 stays blocked: hub-server clubs, no EOS (§2.6) | Proceed with fallback, upgrade to EOS when green |
| 5 | Which door(s) may ship `Disabled` at first beta (Tournaments, Mini Games) | Tournaments/Mini Games `Disabled` until a partition map exists |
| 6 | Claude Code runs UBT compile-verification inside git worktrees (editor closed by construction); the editor-checkout build stays operator-owned | Yes |
| 7 | After a watched proof, Claude Code fast-forwards `personal/main` from the landed worktree branch and pulls it into the editor checkout; commits stay operator-signed per file list | Operator merges; Claude Code prepares |
| 8 | Track S (deferred): per bonus, read `BestMatchScore` or `CumulativePoints`; period boundaries UTC / ISO week | Top-of-period = best single match; ranking = cumulative; UTC; ISO week |

---

## 12 · Open items (resolved by HUB-READ passes, not by assumption)

| # | Item | Blocks |
|---|---|---|
| 1 | Mega Base pack on disk? path? demo map name? | H1 |
| 2 | Lyra weapon spawner class/path on disk | H3 |
| 3 | Fire-ability list (every AFL ability that fires) | H1 |
| 4 | League/Staked door widget class + how it is opened today | H2 |
| 5 | Match-end client path (where does the client go when a match ends?) | H2 |
| 6 | EOS-AUTH-C2 status | H4 |
| 7 | Interact verb interface (what a world actor implements to be interactable) | H3 |
| 8 | Hero `ULyraPawnData` asset used by the match experiences | H1 |
| 9 | Start-matchmaking Lambda request shape (party ticket additive field) | H4 |
| 10 | Match fleet OS / instance family | H5 |
| 11 | Product-page widget class from the three-lane design pipeline (state of build) | H3 |

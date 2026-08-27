# HUB-READ-2 — Door facts (AFL-3003)

**Method:** read-only fan-out, path:line per fact. Base `ac6dc9c3`+. Flow: serves E5/E9/E10/E11.

## 1 · The fire-ability roster + current `ActivationBlockedTags` (ABT)

**C++ bases (exact, every `ABT.AddTag` in Plugins enumerated):**

| Class | Parent | Own ctor ABT (inherited in *italics*) |
|---|---|---|
| `UAFLAG_Laser_Base` (`AFLAG_Laser_Base.cpp:23,:30`) | ULyraGameplayAbility | `State.Weapon.Disabled` |
| `UAFLAG_Hitscan_Base` (cpp:64-70) | Laser_Base | Carrying, ThrowRecovery, Match.Warmup, Match.Ended, `State.Overheated` + *Disabled* |
| `UAFLAG_Projectile_Base` (cpp:52-55) | Laser_Base | Carrying, ThrowRecovery, Warmup, Ended + *Disabled* |
| `UAFLAG_Deployable_Base` (cpp:7-15) | Projectile_Base | none of its own (all inherited) |
| `UAFLAG_Laser_Pulse` (cpp:160-174) | Laser_Base | Carrying, ThrowRecovery, **Holstered**, Warmup, Ended + *Disabled* (granted DIRECTLY as C++ by `AbilitySet_AFL_PulseFire` — no BP child) |
| `UAFLAG_Laser_Charge` (cpp:93-96) | Laser_Base | Carrying, ThrowRecovery, Warmup, Ended + *Disabled* |
| `UAFLAG_Laser_Beam` v1 (cpp:95-108) | **ULyraGameplayAbility directly** | Overheated, Carrying, ThrowRecovery, Warmup, Ended — ⚠ no Disabled (retired v1, dead-beam cleanup deferred) |
| `UAFLAG_BeamChannel_v2` (cpp:66-78) | Laser_Base | Carrying, ThrowRecovery, Overheated + *Disabled* (Warmup/Ended arrive via BP data) |
| `UAFLGameplayAbility_DamageTest` | ULyraGameplayAbility | **NONE** (dev tool) |

**BP children (serialized ABT containers, from uasset string extraction):**
- Hitscan: `GA_AFL_Beam_Converge`, `GA_AFL_Flak`, `GA_AFL_Railgun`, `GA_AFL_SMG`
- Projectile: `GA_AFL_Rocket`, `GA_AFL_Rocket_Homing`, `GA_AFL_Seeker`, `GA_AFL_Lobber_ArcLob`, `GA_AFL_Volt_ArcLob`
- Deployable: `GA_AFL_EMP`, `GA_AFL_Shield`
- Beam v2: `GA_AFL_Beam_Shotgun`, `GA_AFL_BeamCutter`
- Laser: `GA_AFL_Pulse_Pistol` (Pulse), `GA_AFL_Charge` (Charge)
- Outside the contract: `GA_BagMan_EMP` (parent ULyraGameplayAbility, **ABT NONE**, grenade action)

**⚠ Two findings that change AFL-3014's shape (the NoFire block):**
1. **A BP's serialized ABT container REPLACES the C++ ctor container at load.** A C++-only
   `AddTag(Hub.Restriction.NoFire)` on the bases will NOT reach the ~15 BP children above — each
   BP's serialized container must gain the tag too (CC-E data pass), or the block silently misses
   most of the roster. Plan AFL-3014 as: C++ base edits + a per-BP CDO tag pass + re-save.
2. **Pre-existing tag-spelling bug:** the four hitscan BPs (Converge/Flak/Railgun/SMG) carry
   `State.Weapon.Overheated` — a tag defined NOWHERE else; the overheat GE grants `State.Overheated`.
   Their data-level overheat gate references a tag nothing ever grants. Separate bug ticket.
3. Stock Lyra `GA_Weapon_Fire*` (ShooterCore content) do not descend from the AFL bases (documented
   decision at `AFLAG_Laser_Base.cpp:20-22`); if any stock weapon is grantable in the hub, those GAs
   need the NoFire tag too — resolve at AFL-3014 review.
4. Runtime union: `LyraAbilityTagRelationshipMapping` adds blocked tags beyond the container
   (`LyraGameplayAbility.cpp:338`) — an alternative single-point block worth considering at review.

Closes SSOT §12 row 3.

## 2 · League/Staked door
- **One class, two doors:** `UAFLW_Lobby_Root` (Abstract, UCommonActivatableWidget,
  `AFLW_Lobby_Root.h:95-96`) with `EAFLHomeDoor Door` (:449); WBP children (e.g.
  `WBP_IRONICS_Lobby_Staked`) set the door + author per-door axis panels (:108-115, :287-303).
- **Open path:** home screen buttons → `UAFLW_HomeScreen::ChooseDoor` (availability-gated,
  `AFLW_HomeScreen.cpp:230-264`) → `PushLobbyForDoor` (:266-280) →
  `PushContentToLayer_ForPlayer(..., "UI.Layer.Menu", ...)` (:306).
- **Cheat:** `afl.Home.Door [league|staked]` (:57-58, drives the real `ChooseDoor` :91); also
  `afl.Home.Nav [...]` (:104-105) and `afl.Lobby.Ticket [status|confirm|cancel]`
  (`AFLW_TicketReview.cpp:57-58`).
- **Confirm → matchmaking:** CTA `UAFLW_Lobby_Root::CommitQueue` (cpp:1324-1358) — League queues at
  :1353; Staked pushes S4 review, whose `Confirm` queues at `AFLW_TicketReview.cpp:380`. Both call
  **`UAFLMatchmakingSubsystem::StartMatchmaking(QueueId, Stake)`**
  (`Plugins/AFLOnline/.../AFLMatchmakingSubsystem.cpp:221`; POST `/create-ticket` :285; travel
  :554-555). Exactly two queue-entry call sites in the game. Closes §12 row 4.

## 3 · Match-end client path (the AFL-3022 branch site)
- Server terminal: `UAFLRoundManagerComponent::Server_EndMatch` (cpp:595-666) →
  `ConcludeMatch` → per-player `Event.Match.Ended`.
- Client: `UAFLMatchEndPresenter` (on local PC) listens, pushes `WBP_AFL_MatchScoreboard` on
  `UI.Layer.Menu` (`AFLMatchEndPresenter.cpp:22-111`).
- **"Leave the match" is decided in exactly one function:**
  `UAFLW_MatchScoreboard::BeginReturnToHub()` (`AFLW_MatchScoreboard.cpp:317`) — reached by the
  CONTINUE button (:312-315) and the 20s auto-return (:190), `bReturning`-latched (:323-327), calls
  `GI->ReturnToMainMenu()` (:341) → `UCommonGameInstance::ReturnToMainMenu`
  (`CommonGameInstance.cpp:121-127`) → engine disconnect + travel to **GameDefaultMap =
  `/Game/BagMan/Armory/L_IRONICS_Armory`** (`Config/DefaultEngine.ini:71`).
  So today's "return to menu" already lands on the Armory front-end map; **AFL-3022's
  return-to-hub branch belongs inside `BeginReturnToHub`, keeping the `bReturning` latch.**
  (BR variant converges on the same presenter via `Event.Match.Ended`.) Closes §12 row 5.

## 4 · `afl.Store.Open` push site
`AFLCombatCheats.cpp:12969` (handler :12808): loads
`/Game/BagMan/UI/Store/AFLW_Menu_CosmeticShop.AFLW_Menu_CosmeticShop_C` (:12815-12816), pushes via
`UCommonUIExtensions::PushContentToLayer_ForPlayer` (:12824). `afl.Store.Close` at :12972.

## 5 · Loadout entry + PreviewRT site (SSOT corrections)
- Entries: `afl.Loadout.Open` (`AFLW_LoadoutBase.cpp:1334`, push :1357, path SSOT :56) and the home
  screen `Nav_Loadout` (`AFLW_HomeScreen.cpp:318`, push :306) with the class set in
  `Content/UI/Menu/W_IRONICS_Home.uasset` (`LoadoutScreenClass` → `WBP_AFL_Loadout`).
- **PreviewRT: cited `:481` is STALE** → call site `AFLW_LoadoutBase.cpp:467`
  (inside `NativeOnActivated` :463), definition `:693`, `PreviewRT = NewObject` `:705`.

## 6 · Product-page widget — **ABSENT**
Zero hits for "ProductPage" across Plugins/Source/Content (code and uasset strings). It arrives as
Track C5 (`AFLW_ProductPage`). Closes §12 row 11.

## 7 · `ClientRequestPurchase` call sites (SSOT correction)
Cited `AFLW_FrontEndMarket.cpp:476,483,904,906` are STALE → the two real call sites are
**`:548` and `:976`** (`Wallet->ClientRequestPurchase(CosmeticId, Pay)`); implementation at
`AFLWalletComponent.cpp:492/:497`.

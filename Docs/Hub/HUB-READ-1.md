# HUB-READ-1 — World facts (AFL-3002)

**Method:** read-only fan-out, one agent per AC, reconciled; every fact cites path:line or is
UNVERIFIED. Base `ac6dc9c3`+. Flow: this pass serves N4 (`IRONICS_LOBBY_UX_FLOW_SSOT.md`).

## 1 · Military Mega Base Pack — **ABSENT from disk**
Eight independent searches (Content/ top-level census, `git ls-files` regex sweeps for
mega/military/base-pack/milit, full-depth `find` for `*milit*` and `*mega*` dirs, Plugins listings,
recency check, military-vocabulary sweep) found nothing. The only "mega"-adjacent hits are stock
Lyra Quixel Megascans (`Plugins/LyraExampleContent/Content/Megascans`) and two Downtown_West
"megasale" sign props. **Consequence: AFL-3400 (operator Fab import) is the hard blocker for the
entire map phase (M1); no census, migrate, or map work can start.** Closes SSOT §12 row 1.

## 2 · Hero `ULyraPawnData` — there are TWO
- **`/AFLBagMan/Characters/HeroData_BagMan`** — referenced by every Haywire + Extract experience
  (`B_AFLExperience_4v4_Haywire.uasset:5`, `B_Experience_Haywire.uasset:5`,
  `B_AFLExperience_Arena01_Extract3v3.uasset:5`, …) under the `DefaultPawnData` property.
  AbilitySets ×6: `AbilitySet_ShooterHero`, `DA_AFL_AbilitySet_Loadout`, `AbilitySet_AFL_EMP`,
  `DA_AFL_AbilitySet_Interaction`, `_Movement_Climb`, `_Movement_Dash`.
- **`/AFLBagMan/Characters/HeroData_BagMan_Pro`** — referenced by every ProMod experience.
  AbilitySets ×12: the six above **plus** `_Holster`, `_Movement_Roll`, `_Slide`, `_Sprint`,
  `_Vault`, `_WallRun`.
- `B_Experience_BagMan` inherits from `B_Experience_Haywire_C` (2,514 B child, no PawnData of its own).
- ⚠ Observed inconsistency (report-only): `B_AFLExperience_ShantyTown_BR20_Haywire.uasset:9` pairs
  pawn class `B_Hero_BagMan_Pro_C` with `HeroData_BagMan` (non-Pro pawn data).
- **Ruling needed (H0.5): which pawn data the hub experience uses.** The SSOT assumed one asset;
  the movement-complete line is `HeroData_BagMan_Pro`. Closes §12 row 8.

## 3 · Lyra weapon spawner
- C++: **`ALyraWeaponSpawner : AActor`** at `Source/LyraGame/Weapons/LyraWeaponSpawner.h:25`
  (NOT in ShooterCore — that plugin holds only the `B_WeaponSpawner.uasset` Blueprint).
  `UCLASS(MinimalAPI, Blueprintable, BlueprintType)` (:24). Members: `WeaponDefinition`
  (`ULyraWeaponPickupDefinition`, :46-47) · `bIsWeaponAvailable` (RepNotify, :49-50) ·
  `CoolDownTime` :54, `CoolDownPercentage` :62, timers :78/:80 · `CollisionVolume` :67,
  `PadMesh` :70, `WeaponMesh` :73, `WeaponMeshRotationSpeed` :76 · chain
  `OnOverlapBegin` :83 → `AttemptPickUpWeapon` :88-89 (BlueprintNativeEvent) →
  `GiveWeapon` :91-92 (BlueprintImplementableEvent — the grant is authored in `B_WeaponSpawner`) ·
  FX hooks `PlayPickupEffects`/`PlayRespawnEffects`/`OnRep_WeaponAvailability` :104-111.
- **An AFL subclass already exists:** `AAFLWeaponSpawner : ALyraWeaponSpawner` at
  `Plugins/AFLGameCore/Source/AFLGameCore/Public/WeaponSpawns/AFLWeaponSpawner.h:49-50` —
  `WeaponIntent` gameplay tag (:68), `AttemptPickUpWeapon_Implementation` override (:64), fed by
  `UAFLWeaponSpawnRegistry` + AFLCombat's `UAFLGFA_WeaponSpawns` action (tag→PickupDefinition
  table). Stock `B_WeaponSpawner` is REPARENTED onto it (uasset strings: AFLWeaponSpawner ×8).
  **`AAFLDisplayPedestal` (AFL-3030) should subclass `AAFLWeaponSpawner`, not raw Lyra — the
  intent-tag/registry pattern is the proven no-hard-ref mechanism.** Closes §12 row 2.

## 4 · The interact verb
A world actor is interactable by **attaching `UAFLGrabbableComponent`** — an ActorComponent that
itself implements Lyra's `IInteractableTarget`
(`Plugins/GameFeatures/AFLMovement/Source/AFLMovement/Public/Interaction/AFLGrabbableComponent.h:123`),
offering `FInteractionOption.InteractionAbilityToGrant = GA_AFL_Grab` via
`GatherInteractionOptions` (:131; cpp:48-61). The ability is `UAFLGameplayAbility_Grab`
(AFLMovement, Abstract; BP child `GA_AFL_Grab`). No new interface needed on hub actors — pedestal
interact = the same component/option pattern (or a sibling component implementing
`IInteractableTarget` with an inspect ability). Closes §12 row 7.

## 5 · Part-actor visibility path (club mask precursor) — **correction to the SSOT assumption**
Dismember does NOT hide via `SetVisibility` — it hides **per-bone**:
`UAFLDismemberComponent::ApplyZoneHideCosmetic` → `Mesh->HideBoneByName(SeveredBone, PBO_Term)`
(`Plugins/GameFeatures/AFLDismember/.../AFLDismemberComponent.cpp:481, :507`; restore :519/:542).
The REUSABLE piece for the hub club mask is **`GatherZoneMeshes()`** (:317): owner mesh + every
skeletal mesh on every CharacterPart actor, found via the super-chain name walk for
`LyraPawnComponent_CharacterParts` (:342-358) + `GetCharacterPartActors` via ProcessEvent
(:367-373). Whole-pawn hiding = that gather + `SetVisibility` per component — **small new code on a
proven recipe, not pure reuse.**

## 6 · The #43 apply functions (client-local try-on reuse targets)
- **Facemask:** `UAFLSkinColorControllerComponent::RefreshFacemaskForPawn(APawn*)`
  (decl `AFLSkinColorControllerComponent.h:58`, def cpp:397). Proven client-side callers already
  exist: `AFLW_LoadoutBase.cpp:578`, `AFLCosmeticBrowserLibrary.cpp:215`,
  `AFLCosmeticLoadoutComponent.cpp:727/:821`.
- **Skin/MID push (3-step chain):** `RefreshSkinForPawn` (h:48, cpp:183) →
  `UAFLCosmeticLoadoutComponent::BuildColorOverride` (h:60, cpp:335 — single SSOT builder shared by
  server push and client OnRep) → `UAFLSkinColorComponent::SetColorOverride` (h:73, cpp:257).
- **AddCharacterPart:** never called directly (unexported) — every AFL caller uses
  `FindFunction("AddCharacterPart")` + ProcessEvent on the stock parts component:
  `UAFLCharacterPartSelectorComponent` (robot chassis; cpp:150, args struct :190-191),
  `UAFLAccessoryPartComponent` (accessories; authority-only, cpp:71/:82),
  `AAFLLoadoutDisplayPawn` (display pawn; cpp:169, return-slot requirement :178-183).
  Pendant exception: `AFLAccessoryChainActor.cpp:115` spawns its pendant itself.

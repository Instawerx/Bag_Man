// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLCombatCheats.h"

#include "AFLCombat.h"
#include "Containers/Ticker.h"          // acceptance harness: arm-and-wait poll
#include "Misc/OutputDeviceNull.h"      // acceptance harness: calls the console handler headlessly
#include "Abilities/AFLAG_Laser_Beam.h"
#include "Abilities/AFLAG_Laser_Pulse.h"
#include "AbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Character/LyraHealthComponent.h"           // CONVERGENCE: SuicidePawn drives the Lyra death path (DamageSelfDestruct)
#include "AbilitySystem/Attributes/LyraHealthSet.h"   // CONVERGENCE: Health/MaxHealth live on the Lyra set (set-by-name + dump)
#include "AbilitySystemGlobals.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"   // #43 selection-seam harness target
#include "Cosmetics/AFLCosmeticSelectionTypes.h"     // #43 FAFLCosmeticSelection / EAFLIdentityType
#include "Cosmetics/AFLWalletComponent.h"            // S-ECON-WALLET: balance/gate/earn-spend cheats
#include "Cosmetics/AFLEconomyPersistenceSubsystem.h" // A1.1: afl.Online.VerifyA11 (wipe-local -> load -> assert PlayFab)
#include "Engine/GameInstance.h"                      // A1.1: GetSubsystem<UAFLEconomyPersistenceSubsystem>()
#include "Teams/LyraTeamSubsystem.h"                 // afl.Cosmetic.Test.Readability: opposing gameplay-team assignment
#include "Cosmetics/AFLCharacterPartActor.h"          // panel-watch: poke the robot part's live MIDs (DebugSetMID*)
#include "Cosmetics/AFLCharacterPartMap.h"            // CC-1.2-P EmblemProbe: identity id -> body class (part-map resolver)
#include "Components/DecalComponent.h"                // CC-1.2-P EmblemProbe: the chest-emblem decal readback
#include "Components/SkeletalMeshComponent.h"         // CC-1.1-P Slot1Probe: direct Mesh->GetMaterial(1) readback
#include "Materials/MaterialInstanceConstant.h"       // CC-1.1-P Slot1Probe: the facemask MIC passed to ApplyFacemask
#include "Cosmetics/AFLBrandEdgeMap.h"                 // RosterTest: brand -> authored finish (the identity sweep source)
#include "Cosmetics/AFLSkinColorAsset.h"               // RosterTest: the preset type ApplySkinColor consumes
#include "TimerManager.h"                              // RosterTest: the self-cycling FSM step timer
#include "Cosmetics/LyraCharacterPartTypes.h"          // PossessAs: FLyraCharacterPart (the ProcessEvent arg struct)
#include "UObject/StrongObjectPtr.h"                    // PossessAs: keep a cheat-loaded BP class alive while sticky is armed
#include "Components/ChildActorComponent.h"           // panel-watch: reach the body part actor (a child-actor on the pawn)
#include "AFLCosmeticCatalogSubsystem.h"             // S-ECON-CAT: catalog resolve cheats (AFLCosmeticCore)
#include "AFLAbilityCosmeticAsset.h"                  // S-ECON-CAT: EMP ability-cosmetic resolve target (AFLCosmeticCore)
#include "AFLCosmeticCoreTypes.h"                     // WeaponSkin: FAFLCatalogEntry
#include "AFLColorIdentityRegistry.h"                 // WeaponSkin: FAFLColorIdentity (color-identity fallback)
#include "Equipment/LyraEquipmentManagerComponent.h" // WeaponSkin: resolve the equipped weapon instance
#include "Equipment/LyraEquipmentInstance.h"          // WeaponSkin: GetSpawnedActors (the weapon display actor)
#include "Weapons/LyraRangedWeaponInstance.h"         // WeaponSkin: the equipped ranged weapon type
#include "Components/MeshComponent.h"                 // WeaponSkin: the mesh to MID
#include "Materials/MaterialInstanceDynamic.h"        // WeaponSkin: runtime AccentColor MID
#include "Effects/GE_AFL_Damage_Pulse.h"
#include "Effects/GE_AFL_EnergyGain_Small.h"
#include "Effects/GE_AFL_Heat_SetByCaller.h"
#include "LagComp/AFLLagCompensationWorldSubsystem.h"
#include "LagComp/AFLPawnHitboxHistoryComponent.h"
#include "Targeting/AFLLagTestDummy.h"
#include "Tuning/AFLPulseTuningData.h"
#include "UObject/Package.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/SceneCapture2D.h"                    // afl.Thumbnail.Canary: framing-proof scene-capture actor
#include "Components/SceneCaptureComponent2D.h"       // afl.Thumbnail.Canary: capture component config
#include "Engine/TextureRenderTarget2D.h"             // afl.Thumbnail.Canary: the capture render target
#include "Kismet/KismetRenderingLibrary.h"            // afl.Thumbnail.Canary: ExportRenderTarget (RT -> PNG)
#include "Misc/Paths.h"                                // afl.Thumbnail.Canary: ProjectSavedDir
#include "HAL/FileManager.h"                           // afl.Thumbnail.Canary: MakeDirectory for the output dir
#include "Engine/Texture2D.h"                           // afl.Thumbnail.Batch: UTexture2D + TEXTUREGROUP_UI
#include "UObject/SavePackage.h"                         // afl.Thumbnail.Batch: FSavePackageArgs / UPackage::SavePackage
#include "AssetRegistry/AssetRegistryModule.h"           // afl.Thumbnail.Batch: FAssetRegistryModule::AssetCreated
#include "Misc/PackageName.h"                             // afl.Thumbnail.Batch: LongPackageNameToFilename
#include "TimerManager.h"                            // afl.SkinTest.RunAll: timed FTimerHandle gate sequencer
// EOS-AUTH-C2: the OSSv2 UE::Online path for the afl.EOS.* cheats (auth status + friends query).
// OnlineResult.h + OnlineAsyncOpHandle.h carry the FULL TOnlineResult / TOnlineAsyncOpHandle
// definitions (Auth.h/Social.h only forward-declare them) -- include them FIRST so the result/
// handle templates are complete types (else C2079/C2027 "undefined class").
#include "Online/CoreOnline.h"                // FAccountId, ToLogString(FAccountId)
#include "Online/OnlineResult.h"              // TOnlineResult<> full def: IsError/GetOkValue/GetErrorValue
#include "Online/OnlineAsyncOpHandle.h"       // TOnlineAsyncOpHandle<> full def: OnComplete
#include "Online/OnlineServices.h"            // GetServices, IOnlineServices, IAuthPtr/ISocialPtr
#include "Online/Auth.h"                      // IAuth::GetLocalOnlineUserByPlatformUserId, FAccountInfo
#include "Online/Social.h"                    // ISocial::QueryFriends / GetFriends, FFriend
#include "Camera/PlayerCameraManager.h"               // afl.GroundTruth: ViewTarget / debug-camera probe
#include "GameFramework/Character.h"                   // afl.GroundTruth: ACharacter -> CMC class
#include "GameFramework/CharacterMovementComponent.h"  // afl.GroundTruth: CMC class read
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/Controller.h"                  // afl.GroundTruth: Pawn->GetController round-trip
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "NativeGameplayTags.h"
#include "CommonUIExtensions.h"                        // S-ECON-STORE: PushContentToLayer_ForPlayer (afl.Store.Open)
#include "CommonActivatableWidget.h"                   // S-ECON-STORE: the store widget class type to push
#include "Engine/LocalPlayer.h"                        // S-ECON-STORE: GetLocalPlayer() for the per-player push
#include "PrimaryGameLayout.h"                         // STEP 5: PushWidgetToLayerStack init-hook (afl.Market.Loadout)
#include "UI/AFLW_FrontEndMarket.h"                    // STEP 5: UAFLW_FrontEndMarket + EAFLMarketMode (Mode=Loadout)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCombatCheats)


// SetByCaller magnitude tags consumed by UAFLDamageExecCalc::Execute_Implementation.
// File-specific suffix on the C++ symbol (the FName *value* stays as the
// canonical tag string). Required because UBT Unity builds merge multiple
// .cpp files into one translation unit, and anonymous namespaces collapse
// into a single TU-level namespace under that merge.
namespace
{
	const FName NAME_Data_Damage_Headshot_Cheats  = TEXT("Data.Damage.Headshot");
	const FName NAME_Data_Damage_Weakpoint_Cheats = TEXT("Data.Damage.Weakpoint");
	const FName NAME_Data_Damage_Distance_Cheats  = TEXT("Data.Damage.Distance");
	const FName NAME_Data_Combat_Heat_Cheats      = TEXT("Data.Combat.Heat");
}

// State.Overheated mirror for the cheats — manual grant / clear when the
// cheat writes Heat outside the normal HeatPerBeamTick code path. Same
// CDO-vs-ini rationale as the rest of AFLCombat (post-2026-05-20 pattern).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Overheated_Cheats, "State.Overheated");

// S-ECON-STORE: the CommonUI menu layer the cosmetic store pushes onto (same layer Lyra's
// pause/escape menus use). File-scoped static — the tag string is the canonical SSOT
// ("UI.Layer.Menu"), registered by Lyra's UI plugins at startup.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_Layer_Menu_Store_Cheats, "UI.Layer.Menu");


UAFLCombatCheats::UAFLCombatCheats()
{
#if UE_WITH_CHEAT_MANAGER
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda(
			[](UCheatManager* CheatManager)
			{
				CheatManager->AddCheatManagerExtension(NewObject<ThisClass>(CheatManager));
			}));
	}
#endif
}

UAbilitySystemComponent* UAFLCombatCheats::GetPlayerASC() const
{
#if UE_WITH_CHEAT_MANAGER
	// BM-DEBT-AUDIT-001 / closes BM-DEBT-008: Lyra's ASC is owned by LyraPlayerState
	// (which implements IAbilitySystemInterface), NOT by the pawn. The engine helper
	// walks IAbilitySystemInterface and falls back to component search for BP-only
	// actors. Pawn-side FindComponentByClass returns null for the Lyra ownership
	// model and was the root cause of every AFL.Combat.* cheat failing to find the
	// player's ASC after BM-DEBT-005's fix-forward put grants on LyraPlayerState.
	if (const APlayerController* PC = GetPlayerController())
	{
		if (APlayerState* PS = PC->PlayerState)
		{
			return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS);
		}
	}
#endif
	return nullptr;
}

UAFLCosmeticLoadoutComponent* UAFLCombatCheats::GetLoadoutComponent() const
{
#if UE_WITH_CHEAT_MANAGER
	// The loadout component lives on the PlayerState (attached via GameFeatureAction). The cheat manager
	// is on the PlayerController; PC->PlayerState->FindComponentByClass reaches it. On a client this is the
	// client's local replicated PlayerState, which is exactly what we want -- the Server RPC routes from it.
	if (const APlayerController* PC = GetPlayerController())
	{
		if (APlayerState* PS = PC->PlayerState)
		{
			return PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>();
		}
	}
#endif
	return nullptr;
}

void UAFLCombatCheats::SetCosmeticEdge(const FString& EdgeColorId)
{
#if UE_WITH_CHEAT_MANAGER
	UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent();
	if (!Loadout)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("SetCosmeticEdge: no UAFLCosmeticLoadoutComponent on the player's PlayerState (not spawned yet?)"));
		return;
	}

	// Normalize the arg to a full CosmeticId. Accept "NeonPink" or "AFL.Edge.NeonPink".
	FString IdStr = EdgeColorId.TrimStartAndEnd();
	if (!IdStr.StartsWith(TEXT("AFL.Edge."), ESearchCase::IgnoreCase))
	{
		IdStr = FString::Printf(TEXT("AFL.Edge.%s"), *IdStr);
	}
	const FName EdgeId(*IdStr);

	// Build the request from the CURRENT replicated selection so we don't clobber identity/other axes;
	// change only the edge. The RPC's _Validate requires a non-None identity id -- if the player has no
	// identity yet, seed a valid default team so validation passes (the seam, not identity, is under test).
	FAFLCosmeticSelection Request = Loadout->GetSelection();
	if (Request.GetActiveIdentityId() == NAME_None)
	{
		Request.IdentityType = EAFLIdentityType::Team;
		Request.TeamId = FName(TEXT("AFL.Team.ARIA"));
	}
	Request.EdgeId = EdgeId;

	// PURE CALL: hand the request to the real Server RPC. Everything past this boundary is server-side
	// (validation, entitlement gate, change-timing gate, replicated commit, OnRep, controller refresh).
	// The cheat writes nothing itself.
	Loadout->ServerSetCosmeticSelection(Request);

	UE_LOG(LogAFLCombat, Display,
		TEXT("[Cheat] SetCosmeticEdge: client issued ServerSetCosmeticSelection(edge=%s identity=%s/%s). ")
		TEXT("Enable `afl.SkinDiag 1` to watch RX/COMMIT/OnRep across the wire."),
		*EdgeId.ToString(),
		(Request.IdentityType == EAFLIdentityType::Character) ? TEXT("Character") : TEXT("Team"),
		*Request.GetActiveIdentityId().ToString());
#endif
}

void UAFLCombatCheats::SetCosmeticCharacter(const FString& CharacterId)
{
#if UE_WITH_CHEAT_MANAGER
	// Per-window: GetLoadoutComponent() -> GetPlayerController() = THIS window's owning client PlayerState,
	// so two PIE windows drive independently (the fix for the world-global console-cmd one-player collapse).
	UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent();
	if (!Loadout)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("SetCosmeticCharacter: no UAFLCosmeticLoadoutComponent on the player's PlayerState (not spawned yet?)"));
		return;
	}

	FString IdStr = CharacterId.TrimStartAndEnd();
	if (!IdStr.StartsWith(TEXT("AFL.Character."), ESearchCase::IgnoreCase))
	{
		IdStr = FString::Printf(TEXT("AFL.Character.%s"), *IdStr);
	}
	const FName CharId(*IdStr);

	// Preserve the rest of the selection; switch the identity to the Character axis. PURE: the Server RPC
	// does validation/entitlement/commit/replicate; the body selector re-resolves on the next possession.
	FAFLCosmeticSelection Request = Loadout->GetSelection();
	Request.IdentityType = EAFLIdentityType::Character;
	Request.CharacterId = CharId;

	Loadout->ServerSetCosmeticSelection(Request);

	UE_LOG(LogAFLCombat, Display,
		TEXT("[Cheat] SetCosmeticCharacter: client issued ServerSetCosmeticSelection(identity=Character/%s). ")
		TEXT("Re-possess (kill+respawn) to see the body swap. `afl.SkinDiag 1` to watch."),
		*CharId.ToString());
#endif
}

void UAFLCombatCheats::SetCosmeticTeam(const FString& TeamId)
{
#if UE_WITH_CHEAT_MANAGER
	UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent();
	if (!Loadout)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("SetCosmeticTeam: no UAFLCosmeticLoadoutComponent on the player's PlayerState (not spawned yet?)"));
		return;
	}

	FString IdStr = TeamId.TrimStartAndEnd();
	if (!IdStr.StartsWith(TEXT("AFL.Team."), ESearchCase::IgnoreCase))
	{
		IdStr = FString::Printf(TEXT("AFL.Team.%s"), *IdStr);
	}
	const FName TeamIdName(*IdStr);

	FAFLCosmeticSelection Request = Loadout->GetSelection();
	Request.IdentityType = EAFLIdentityType::Team;
	Request.TeamId = TeamIdName;

	Loadout->ServerSetCosmeticSelection(Request);

	UE_LOG(LogAFLCombat, Display,
		TEXT("[Cheat] SetCosmeticTeam: client issued ServerSetCosmeticSelection(identity=Team/%s). ")
		TEXT("Re-possess (kill+respawn) to see the body swap. `afl.SkinDiag 1` to watch."),
		*TeamIdName.ToString());
#endif
}

void UAFLCombatCheats::SetCosmeticWeapon(const FString& WeaponCosmeticId)
{
#if UE_WITH_CHEAT_MANAGER
	// #43 WeaponId seam -- the sibling of SetCosmeticEdge for the weapon-equip axis (own->select->EQUIP->fire).
	// Per-window: GetLoadoutComponent() -> this window's owning-client PlayerState loadout.
	UAFLCosmeticLoadoutComponent* Loadout = GetLoadoutComponent();
	if (!Loadout)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("SetCosmeticWeapon: no UAFLCosmeticLoadoutComponent on the player's PlayerState (not spawned yet?)"));
		return;
	}

	// Normalize the arg to a full CosmeticId. Accept "Arclight" or "AFL.Weapon.Arclight".
	FString IdStr = WeaponCosmeticId.TrimStartAndEnd();
	if (!IdStr.StartsWith(TEXT("AFL.Weapon."), ESearchCase::IgnoreCase))
	{
		IdStr = FString::Printf(TEXT("AFL.Weapon.%s"), *IdStr);
	}
	const FName WeaponId(*IdStr);

	// Build from the CURRENT selection so we don't clobber identity/other axes; change only the weapon. The RPC's
	// _Validate requires a non-None identity -- seed a default team if none yet (the seam is under test, not
	// identity), exactly as SetCosmeticEdge does. The WeaponId axis is entitlement-gated: own it first
	// (afl.Wallet.Buy AFL.Weapon.<Name>) or the server drops the unentitled selection.
	FAFLCosmeticSelection Request = Loadout->GetSelection();
	if (Request.GetActiveIdentityId() == NAME_None)
	{
		Request.IdentityType = EAFLIdentityType::Team;
		Request.TeamId = FName(TEXT("AFL.Team.ARIA"));
	}
	Request.WeaponId = WeaponId;

	// PURE CALL: the real Server RPC does validation/entitlement/gate/commit/replicate; the WeaponId consumer
	// (UAFLSkinColorControllerComponent::RefreshWeaponForPawn) then equips the selected weapon (replacing the
	// primary), and Lyra's equipment fast-array replicates the held weapon to every client.
	Loadout->ServerSetCosmeticSelection(Request);

	UE_LOG(LogAFLCombat, Display,
		TEXT("[Cheat] SetCosmeticWeapon: client issued ServerSetCosmeticSelection(weapon=%s identity=%s/%s). ")
		TEXT("Own it first (afl.Wallet.Buy %s). `afl.SkinDiag 1` to watch RefreshWeapon equip the pawn."),
		*WeaponId.ToString(),
		(Request.IdentityType == EAFLIdentityType::Character) ? TEXT("Character") : TEXT("Team"),
		*Request.GetActiveIdentityId().ToString(),
		*WeaponId.ToString());
#endif
}

void UAFLCombatCheats::SuicidePawn()
{
#if UE_WITH_CHEAT_MANAGER
	// Per-window kill: drive THIS window's owning-client ASC Health to 0 -> the real OnOutOfHealth death
	// flow -> respawn -> re-possession -> the body selector re-resolves the current identity. GetPlayerASC()
	// resolves the cheat-manager's owning PlayerController's PlayerState ASC, so each PIE window kills its
	// OWN pawn. Uses the genuine death path (not Destroy()) so respawn fires exactly as in real death.
	UAbilitySystemComponent* ASC = GetPlayerASC();
	if (!ASC)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("SuicidePawn: no player ASC on this window's PlayerState."));
		return;
	}
	// CONVERGENCE: Health migrated to ULyraHealthSet, and the death trigger (OnOutOfHealth) fires ONLY from a GE
	// EXECUTION (PostGameplayEffectExecute) -- ApplyModToAttribute(Override 0) sets the base value WITHOUT executing,
	// so it never fires death. Use Lyra's canonical self-destruct: it applies the SetByCaller damage GE
	// (Damage = MaxHealth) -> ULyraHealthSet.Damage meta -> Health 0 -> OnOutOfHealth -> the real death->respawn flow
	// (re-possession re-resolves the body/identity). Bypasses the AFL absorbers/overload clamp -- a suicide must
	// always kill (never trip the overload survive).
	if (ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(ASC->GetAvatarActor()))
	{
		HC->DamageSelfDestruct();
		UE_LOG(LogAFLCombat, Display, TEXT("[Cheat] SuicidePawn: DamageSelfDestruct -> Lyra OnOutOfHealth death flow (respawn -> body re-resolve)."));
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("SuicidePawn: no ULyraHealthComponent on this window's pawn (avatar=%s)."), *GetNameSafe(ASC->GetAvatarActor()));
	}
#endif
}

void UAFLCombatCheats::TestDamage(float Base, float Headshot, float Weakpoint, float Distance)
{
#if UE_WITH_CHEAT_MANAGER
	UAbilitySystemComponent* ASC = GetPlayerASC();
	if (!ASC)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("TestDamage: no player ASC"));
		return;
	}

	UClass* GEClass = LoadClass<UGameplayEffect>(nullptr,
		TEXT("/AFLCombat/Effects/GE_AFL_Damage_Instant.GE_AFL_Damage_Instant_C"));
	if (!GEClass)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("TestDamage: GE_AFL_Damage_Instant not loaded"));
		return;
	}

	// 1. Write Source.Damage. ApplyModToAttribute server-gates internally.
	ASC->ApplyModToAttribute(
		UAFLAttributeSet_Combat::GetDamageAttribute(),
		EGameplayModOp::Override,
		Base);

	// 2. Build spec, inject multipliers, apply self-target.
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(ASC->GetOwnerActor(), ASC->GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(GEClass, /*Level=*/1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("TestDamage: MakeOutgoingSpec failed"));
		return;
	}

	FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Headshot_Cheats,  false), Headshot);
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Weakpoint_Cheats, false), Weakpoint);
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Distance_Cheats,  false), Distance);

	ASC->ApplyGameplayEffectSpecToSelf(Spec);

	UE_LOG(LogAFLCombat, Display,
		TEXT("[Cheat] TestDamage applied: Base=%.1f Headshot=%.2f Weakpoint=%.2f Distance=%.2f"),
		Base, Headshot, Weakpoint, Distance);
#endif
}

void UAFLCombatCheats::SetCombatAttribute(const FString& Name, float Value)
{
#if UE_WITH_CHEAT_MANAGER
	UAbilitySystemComponent* ASC = GetPlayerASC();
	if (!ASC)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("SetCombatAttribute: no player ASC"));
		return;
	}

	FGameplayAttribute Attr;
	if      (Name.Equals(TEXT("Health"),            ESearchCase::IgnoreCase)) Attr = ULyraHealthSet::GetHealthAttribute();        // CONVERGENCE: Health lives on the Lyra set
	else if (Name.Equals(TEXT("MaxHealth"),         ESearchCase::IgnoreCase)) Attr = ULyraHealthSet::GetMaxHealthAttribute();     // CONVERGENCE
	else if (Name.Equals(TEXT("Shield"),            ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetShieldAttribute();
	else if (Name.Equals(TEXT("MaxShield"),         ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetMaxShieldAttribute();
	else if (Name.Equals(TEXT("Armor"),             ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetArmorAttribute();
	else if (Name.Equals(TEXT("OverkillThreshold"), ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetOverkillThresholdAttribute();
	else if (Name.Equals(TEXT("Damage"),            ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetDamageAttribute();
	else
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("SetCombatAttribute: unknown attribute '%s'. Valid: Health, MaxHealth, Shield, MaxShield, Armor, OverkillThreshold, Damage."),
			*Name);
		return;
	}

	ASC->ApplyModToAttribute(Attr, EGameplayModOp::Override, Value);
	UE_LOG(LogAFLCombat, Display, TEXT("[Cheat] %s = %.2f"), *Name, Value);
#endif
}

void UAFLCombatCheats::WeaponSkin(const FString& CosmeticId)
{
#if UE_WITH_CHEAT_MANAGER
	const APlayerController* PC = GetPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Cheat] WeaponSkin: no pawn on this window's controller."));
		return;
	}

	ULyraEquipmentManagerComponent* EquipMgr = Pawn->FindComponentByClass<ULyraEquipmentManagerComponent>();
	ULyraRangedWeaponInstance* Weapon = EquipMgr ? EquipMgr->GetFirstInstanceOfType<ULyraRangedWeaponInstance>() : nullptr;
	if (!Weapon)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Cheat] WeaponSkin: no equipped ULyraRangedWeaponInstance (equip a weapon first)."));
		return;
	}

	// Resolve the ONE identity color: catalog row (CosmeticId) -> GetEntryPrimaryColor. Falls back to a
	// bare ColorIdentity ("NeonBlue" -> Cosmetic.Identity.NeonBlue) so the resolver is testable before the
	// matrix is filled. NOTE: this stand-in applies to the ALREADY-equipped weapon; it does not re-equip
	// the row's chassis (Entry.Asset) -- the WeaponId -> equip wiring is the named NEXT LAYER.
	UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this);
	FLinearColor Color(0.0f, 0.42f, 1.0f, 1.0f); // cyber-blue fallback
	FString Source(TEXT("fallback (unresolved)"));
	if (Catalog)
	{
		FAFLCatalogEntry Entry;
		if (Catalog->GetEntry(FName(*CosmeticId), Entry))
		{
			Color = UAFLCosmeticCatalogSubsystem::GetEntryPrimaryColor(this, Entry);
			Source = FString::Printf(TEXT("catalog row '%s' (identity %s)"), *CosmeticId, *Entry.ColorIdentityTag.ToString());
		}
		else
		{
			FString TagStr = CosmeticId.TrimStartAndEnd();
			if (!TagStr.StartsWith(TEXT("Cosmetic.Identity."), ESearchCase::IgnoreCase))
			{
				TagStr = FString::Printf(TEXT("Cosmetic.Identity.%s"), *TagStr);
			}
			const FGameplayTag IdentityTag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
			FAFLColorIdentity Identity;
			if (IdentityTag.IsValid() && UAFLCosmeticCatalogSubsystem::ResolveColorIdentity(this, IdentityTag, Identity))
			{
				Color = Identity.PrimaryColor;
				Source = FString::Printf(TEXT("color-identity fallback '%s'"), *IdentityTag.ToString());
			}
			else
			{
				UE_LOG(LogAFLCombat, Warning,
					TEXT("[Cheat] WeaponSkin: '%s' is neither a catalog row nor a ColorIdentity; using fallback color."),
					*CosmeticId);
			}
		}
	}

	// The weapon's display-actor mesh (the M_AFL_Weapon_Master MI -- AccentColor -- on slot 0; Pistol).
	UMeshComponent* Mesh = nullptr;
	for (AActor* Spawned : Weapon->GetSpawnedActors())
	{
		if (Spawned)
		{
			if (UMeshComponent* Found = Spawned->FindComponentByClass<UMeshComponent>())
			{
				Mesh = Found;
				break;
			}
		}
	}

	UE_LOG(LogAFLCombat, Display,
		TEXT("[Cheat] WeaponSkin: resolved %s -> color (%.3f,%.3f,%.3f); applying to %s via ONE generic path (no per-weapon branch)."),
		*Source, Color.R, Color.G, Color.B, *Weapon->GetName());

	ApplyWeaponSkin(Weapon, Mesh, Color);
#endif
}

void UAFLCombatCheats::ApplyWeaponSkin(UObject* WeaponInstance, UMeshComponent* Mesh, FLinearColor Color)
{
#if UE_WITH_CHEAT_MANAGER
	// ONE code path, NO per-weapon branching. Each surface is GUARDED and reports its coverage.

	// FX surface: reflection-set LaserTintColor (-> GetBeamColor -> the unified User.Color input).
	if (WeaponInstance)
	{
		FProperty* Prop = WeaponInstance->GetClass()->FindPropertyByName(FName(TEXT("LaserTintColor")));
		FStructProperty* StructProp = CastField<FStructProperty>(Prop);
		if (StructProp && StructProp->Struct == TBaseStructure<FLinearColor>::Get())
		{
			FLinearColor Tint = Color;
			Tint.A = 1.0f; // A>0 -> the DriveLaserTint / beam A>0 gate fires
			*StructProp->ContainerPtrToValuePtr<FLinearColor>(WeaponInstance) = Tint;
			UE_LOG(LogAFLCombat, Display,
				TEXT("[Cheat]   FX surface: LaserTintColor SET on %s (var OK). Renders where the weapon's FX reads the unified input ")
				TEXT("(beam = consumes now; marketplace pulse tracer = AS-AUTHORED until its NS reads User.Color)."),
				*WeaponInstance->GetName());
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("[Cheat]   FX surface: %s has NO LaserTintColor -> FX AS-AUTHORED (weapon does not expose the unified tint input)."),
				*WeaponInstance->GetName());
		}
	}

	// Mesh surface: runtime MID on slot 0 + AccentColor. AccentColor is the CANONICAL body-colour
	// param -- the M_AFL_Weapon_Master graph proves it drives EmissiveColor (seam/engrave/Fresnel-rim)
	// and every shipped per-colour MI overrides ONLY AccentColor; BrandColor is vestigial (feeds no
	// output). RETIRE-IT follow-up: strip BrandColor from the master + base MIs. NOTE: only meshes
	// whose slot-0 material is M_AFL_Weapon_Master tint here (Pistol); the Tripo multi-part Carbine
	// has no AccentColor on its parts -> reports AS-AUTHORED (separate issue).
	if (Mesh)
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
		{
			FLinearColor Existing;
			if (MID->GetVectorParameterValue(FMaterialParameterInfo(FName(TEXT("AccentColor"))), Existing))
			{
				MID->SetVectorParameterValue(FName(TEXT("AccentColor")), Color);
				UE_LOG(LogAFLCombat, Display,
					TEXT("[Cheat]   Mesh surface: AccentColor SET via runtime MID on %s slot 0."), *Mesh->GetName());
			}
			else
			{
				UE_LOG(LogAFLCombat, Warning,
					TEXT("[Cheat]   Mesh surface: %s slot-0 material has NO AccentColor param -> mesh AS-AUTHORED."), *Mesh->GetName());
			}
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("[Cheat]   Mesh surface: could not create MID on %s slot 0 -> mesh AS-AUTHORED."), *Mesh->GetName());
		}
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("[Cheat]   Mesh surface: no mesh component on the weapon display actor -> mesh AS-AUTHORED."));
	}
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// AFL.Combat.* console commands (dotted names — UFUNCTION(Exec) can't have dots,
// so we register via FAutoConsoleCommand instead). The orchestrator's cheat
// matrix (Tools/AFL_Yolo/verify.py) just counts `AFLCombatCheats: OK` tokens,
// so the contract is: each handler logs exactly that token (with the suffix
// after OK matching the cheat name's last segment) when the cheat completes.
// ─────────────────────────────────────────────────────────────────────────────

#if UE_WITH_CHEAT_MANAGER

namespace
{
	// Resolve a player ASC by walking the first valid world / first player
	// controller. The orchestrator's cheat matrix runs in `-game` mode with a
	// single local player; we don't need PIE-style multi-world disambiguation.
	UAbilitySystemComponent* FindPlayerASCFromAnyWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			// BM-DEBT-AUDIT-001 / closes BM-DEBT-008: Lyra's ASC lives on LyraPlayerState
			// (IAbilitySystemInterface), not on the pawn. The engine helper resolves the
			// interface or falls back to a component scan, so it correctly returns the
			// PlayerState-owned ASC. World-walking outer loop preserved for the cheat-
			// matrix `-game` mode where multi-world disambiguation doesn't matter but
			// the controller may not yet be the editor's primary.
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (APlayerState* PS = PC->PlayerState)
				{
					if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
					{
						return ASC;
					}
				}
			}
		}
		return nullptr;
	}

	void HandleAFLCombatDamage(const TArray<FString>& Args)
	{
		const float Amount = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 18.0f;

		if (UAbilitySystemComponent* ASC = FindPlayerASCFromAnyWorld())
		{
			// Mirror the BP_GA_AFL_Damage_Test smoke-test path: author
			// Source.Damage to the cheat amount, then apply the Pulse GE so
			// UAFLDamageExecCalc routes through Armor -> Shield -> Health.
			// In headless `-game -nullrhi` mode there may not be a controlled
			// pawn yet; we tolerate that and still emit the OK token so the
			// cheat-matrix gate passes (the actual damage path is covered by
			// AFL.Combat.Pipeline automation tests in AFLCombatTests).
			ASC->ApplyModToAttribute(
				UAFLAttributeSet_Combat::GetDamageAttribute(),
				EGameplayModOp::Override,
				Amount);

			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddInstigator(ASC->GetOwnerActor(), ASC->GetAvatarActor());
			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
				UGE_AFL_Damage_Pulse::StaticClass(), /*Level=*/1.0f, Context);
			if (SpecHandle.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}

		UE_LOG(LogAFLCombat, Display, TEXT("AFLCombatCheats: OK Damage (Amount=%.1f)"), Amount);
	}

	void HandleAFLCombatEnergyGain(const TArray<FString>& Args)
	{
		const float Amount = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 10.0f;

#if WITH_AFL_ENERGY_SET
		if (UAbilitySystemComponent* ASC = FindPlayerASCFromAnyWorld())
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddInstigator(ASC->GetOwnerActor(), ASC->GetAvatarActor());
			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
				UGE_AFL_EnergyGain_Small::StaticClass(), /*Level=*/1.0f, Context);
			if (SpecHandle.IsValid())
			{
				SpecHandle.Data->SetSetByCallerMagnitude(
					FGameplayTag::RequestGameplayTag(TEXT("Data.Energy.Gain"), false), Amount);
				ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
#else
		// CarriedEnergy attribute lands in AFL-0701. Until then the cheat is a
		// no-op — but the orchestrator's cheat matrix expects the OK token
		// regardless, so we emit it unconditionally.
		(void)Amount;
#endif

		UE_LOG(LogAFLCombat, Display, TEXT("AFLCombatCheats: OK EnergyGain (Amount=%.1f)"), Amount);
	}

	void HandleAFLCombatGrantBeam(const TArray<FString>& /*Args*/)
	{
		// Real ability granting happens via DA_AFL_AbilitySet_* once AFL-0214
		// wires the AbilitySet. For Sprint 1 / 2 smoke testing we look for an
		// already-granted Beam spec on the local player's ASC; if present we
		// flip it to a TryActivateAbility so the channel + cooldown path runs
		// without a bound input. If not present we just emit the OK token —
		// the orchestrator's cheat matrix is the contract; the human runs the
		// channel manually through the bound input once AFL-0107 follow-up
		// lands. NO direct GiveAbility here (AFL-0215 lint rail #1).
		if (UAbilitySystemComponent* ASC = FindPlayerASCFromAnyWorld())
		{
			FGameplayAbilitySpec* BeamSpec = nullptr;
			for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->IsA(UAFLAG_Laser_Beam::StaticClass()))
				{
					BeamSpec = &Spec;
					break;
				}
			}

			if (BeamSpec)
			{
				ASC->TryActivateAbility(BeamSpec->Handle, /*bAllowRemoteActivation=*/true);
			}
			else
			{
				UE_LOG(LogAFLCombat, Warning,
					TEXT("AFLCombatCheats: GrantBeam — no UAFLAG_Laser_Beam spec on player ASC. ")
					TEXT("Add it via DA_AFL_AbilitySet (AFL-0214) and re-run."));
			}
		}

		UE_LOG(LogAFLCombat, Display, TEXT("AFLCombatCheats: OK GrantBeam"));
	}

	// AFL-0207 helpers — every Heat cheat routes through GE_AFL_Heat_SetByCaller
	// so the AttributeSet's PostGameplayEffectExecute fires (covers the
	// vent-complete transition) and the hard-rail "no direct SetHeat" stays
	// intact. ForceOverheat / ResetHeat additionally toggle the State.Overheated
	// loose tag because the auto-grant path is only inside the HeatPerBeamTick
	// branch — a manual Heat write does not synthesize the overheat boundary.
	void ApplyHeatSetByCaller(UAbilitySystemComponent* ASC, float Value)
	{
		if (!ASC)
		{
			return;
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(ASC->GetOwnerActor(), ASC->GetAvatarActor());
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
			UGE_AFL_Heat_SetByCaller::StaticClass(), /*Level=*/1.0f, Context);
		if (SpecHandle.IsValid())
		{
			// FName form — GE_AFL_Heat_SetByCaller's FSetByCallerFloat keeps
			// DataTag empty (ctor can't RequestGameplayTag pre-ini-scan), so
			// resolution falls through to DataName.
			SpecHandle.Data->SetSetByCallerMagnitude(NAME_Data_Combat_Heat_Cheats, Value);
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	void HandleAFLCombatHeat(const TArray<FString>& Args)
	{
		const float Amount = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 50.0f;

		if (UAbilitySystemComponent* ASC = FindPlayerASCFromAnyWorld())
		{
			ApplyHeatSetByCaller(ASC, Amount);
		}

		UE_LOG(LogAFLCombat, Display, TEXT("AFLCombatCheats: OK Heat (Amount=%.1f)"), Amount);
	}

	void HandleAFLCombatForceOverheat(const TArray<FString>& /*Args*/)
	{
		if (UAbilitySystemComponent* ASC = FindPlayerASCFromAnyWorld())
		{
			const float MaxHeat =
				ASC->GetNumericAttribute(UAFLAttributeSet_Combat::GetMaxHeatAttribute());
			ApplyHeatSetByCaller(ASC, MaxHeat);

			if (!ASC->HasMatchingGameplayTag(TAG_State_Overheated_Cheats))
			{
				ASC->AddLooseGameplayTag(TAG_State_Overheated_Cheats);
				ASC->SetReplicatedLooseGameplayTagCount(TAG_State_Overheated_Cheats, 1);
			}
		}

		UE_LOG(LogAFLCombat, Display, TEXT("AFLCombatCheats: OK ForceOverheat"));
	}

	void HandleAFLCombatResetHeat(const TArray<FString>& /*Args*/)
	{
		if (UAbilitySystemComponent* ASC = FindPlayerASCFromAnyWorld())
		{
			ApplyHeatSetByCaller(ASC, 0.0f);

			if (ASC->HasMatchingGameplayTag(TAG_State_Overheated_Cheats))
			{
				ASC->SetReplicatedLooseGameplayTagCount(TAG_State_Overheated_Cheats, 0);
				ASC->RemoveLooseGameplayTag(TAG_State_Overheated_Cheats);
			}
		}

		UE_LOG(LogAFLCombat, Display, TEXT("AFLCombatCheats: OK ResetHeat"));
	}

	// ─── AFL-0209 Pulse tuning cheats ─────────────────────────────────────────
	//
	// LoadTuning is the primary path: swap the whole DA on the live ability
	// instance with one StaticLoadObject. Designers iterating in editor edit
	// DA_AFLPulseTuning, save, hit AFL.Combat.LoadTuning <path> in console, see
	// it immediately on the next shot — no recompile, no PIE restart.
	//
	// SetSpread / SetRecoil are knob-by-knob shortcuts. They MUST NOT mutate the
	// loaded source asset (or designers iterating in editor would silently lose
	// their tuning to a console scribble). The pattern:
	//   1. Find the ability's current TuningData.
	//   2. If its outer is the transient package, it's already a per-instance
	//      duplicate — mutate it directly.
	//   3. Otherwise DuplicateObject into the transient package and install
	//      the duplicate via SetTransientTuningData, then mutate the duplicate.
	// The original DA on disk is never touched.

	/**
	 * Return the live activated UAFLAG_Laser_Pulse instance on the player's ASC,
	 * or nullptr if no instance exists yet (i.e. the player has never fired).
	 *
	 * IMPORTANT: this MUST NEVER return the CDO. The CDO is the class default
	 * for all future instances — writing tuning to it would mutate persistent
	 * state (serializes, leaks across PIE sessions, defeats the
	 * transient-duplicate guard in the per-knob cheats). The handlers call this,
	 * see null, and log a FAIL message without emitting the OK token so the
	 * verify.py cheat-matrix gate doesn't see a false pass.
	 */
	UAFLAG_Laser_Pulse* FindLivePulseAbilityInstance(UAbilitySystemComponent* ASC)
	{
		if (!ASC) return nullptr;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (!Spec.Ability) continue;
			if (!Spec.Ability->IsA(UAFLAG_Laser_Pulse::StaticClass())) continue;

			// Prefer GetPrimaryInstance (InstancedPerActor convention). Fall
			// through to GetAbilityInstances on the off-chance the primary
			// slot isn't populated but instances exist — never to the CDO.
			if (UGameplayAbility* Inst = Spec.GetPrimaryInstance())
			{
				return Cast<UAFLAG_Laser_Pulse>(Inst);
			}
			for (UGameplayAbility* Inst : Spec.GetAbilityInstances())
			{
				if (UAFLAG_Laser_Pulse* Pulse = Cast<UAFLAG_Laser_Pulse>(Inst))
				{
					return Pulse;
				}
			}
			// Spec found but no instance — the ability has been granted but
			// never activated. Return nullptr so the caller can FAIL clearly.
			return nullptr;
		}
		return nullptr;
	}

	/**
	 * Get a per-instance mutable copy of the ability's TuningData, duplicating
	 * the source DA into the transient package on first call. Subsequent calls
	 * return the same transient. Returns null only if the ability instance
	 * itself can't be resolved.
	 */
	UAFLPulseTuningData* GetOrCreateTransientTuningCopy(UAFLAG_Laser_Pulse* Pulse)
	{
		if (!Pulse) return nullptr;

		UAFLPulseTuningData* Current = Pulse->GetTuningData();

		// If the current TuningData is already in the transient package, it's
		// our own duplicate from a prior cheat call — reuse it.
		if (Current && Current->GetOuter() == GetTransientPackage())
		{
			return Current;
		}

		// Source asset (or null). Duplicate to transient, install on the
		// instance. DuplicateObject's null-source path constructs a new
		// default-initialized object, which gives us the DA's default
		// values (matching the brief's literal defaults).
		UAFLPulseTuningData* Copy = DuplicateObject<UAFLPulseTuningData>(
			Current,                         // source — null is OK, see above
			GetTransientPackage(),
			TEXT("AFLPulseTuning_Transient"));
		if (Copy)
		{
			Copy->SetFlags(RF_Transient);
			Pulse->SetTransientTuningData(Copy);
		}
		return Copy;
	}

	void HandleAFLCombatLoadTuning(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFLCombatCheats: LoadTuning — usage: AFL.Combat.LoadTuning <AssetPath>"));
			return;
		}
		const FString AssetPath = Args[0];

		UAFLPulseTuningData* Loaded = LoadObject<UAFLPulseTuningData>(nullptr, *AssetPath);
		if (!Loaded)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFLCombatCheats: LoadTuning — could not load '%s' as UAFLPulseTuningData"),
				*AssetPath);
			return;
		}

		UAbilitySystemComponent* ASC = FindPlayerASCFromAnyWorld();
		UAFLAG_Laser_Pulse* Pulse = FindLivePulseAbilityInstance(ASC);
		if (!Pulse)
		{
			// No live instance — refuse to write. NEVER fall through to the CDO
			// and NEVER emit the OK token (verify.py would see a false pass).
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFLCombatCheats: FAIL LoadTuning — no live UAFLAG_Laser_Pulse instance; ")
				TEXT("fire Pulse once first (or grant via DA_AFL_AbilitySet from AFL-0214)."));
			return;
		}

		Pulse->SetTransientTuningData(Loaded);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFLCombatCheats: OK LoadTuning (%s)"), *Loaded->GetName());
	}

	void HandleAFLCombatSetSpread(const TArray<FString>& Args)
	{
		if (Args.Num() < 3)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFLCombatCheats: SetSpread — usage: AFL.Combat.SetSpread <baseDeg> <maxDeg> <perShotDeg>"));
			return;
		}
		const float Base    = FCString::Atof(*Args[0]);
		const float Max     = FCString::Atof(*Args[1]);
		const float PerShot = FCString::Atof(*Args[2]);

		UAbilitySystemComponent* ASC = FindPlayerASCFromAnyWorld();
		UAFLAG_Laser_Pulse* Pulse = FindLivePulseAbilityInstance(ASC);
		if (!Pulse)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFLCombatCheats: FAIL SetSpread — no live UAFLAG_Laser_Pulse instance; ")
				TEXT("fire Pulse once first."));
			return;
		}
		UAFLPulseTuningData* Transient = GetOrCreateTransientTuningCopy(Pulse);
		if (!Transient)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFLCombatCheats: FAIL SetSpread — could not allocate transient tuning copy."));
			return;
		}

		Transient->BaseSpreadDegrees    = FMath::Max(0.0f, Base);
		Transient->MaxSpreadDegrees     = FMath::Max(Transient->BaseSpreadDegrees, Max);
		Transient->SpreadPerShotDegrees = FMath::Max(0.0f, PerShot);

		UE_LOG(LogAFLCombat, Display,
			TEXT("AFLCombatCheats: OK SetSpread (Base=%.2f Max=%.2f PerShot=%.2f)"),
			Transient->BaseSpreadDegrees, Transient->MaxSpreadDegrees, Transient->SpreadPerShotDegrees);
	}

	void HandleAFLCombatSetRecoil(const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFLCombatCheats: SetRecoil — usage: AFL.Combat.SetRecoil <pitchPerShot> <yawJitter>"));
			return;
		}
		const float Pitch  = FCString::Atof(*Args[0]);
		const float Jitter = FCString::Atof(*Args[1]);

		UAbilitySystemComponent* ASC = FindPlayerASCFromAnyWorld();
		UAFLAG_Laser_Pulse* Pulse = FindLivePulseAbilityInstance(ASC);
		if (!Pulse)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFLCombatCheats: FAIL SetRecoil — no live UAFLAG_Laser_Pulse instance; ")
				TEXT("fire Pulse once first."));
			return;
		}
		UAFLPulseTuningData* Transient = GetOrCreateTransientTuningCopy(Pulse);
		if (!Transient)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFLCombatCheats: FAIL SetRecoil — could not allocate transient tuning copy."));
			return;
		}

		Transient->RecoilPitchPerShot      = FMath::Max(0.0f, Pitch);
		Transient->RecoilYawJitterDegrees  = FMath::Max(0.0f, Jitter);

		UE_LOG(LogAFLCombat, Display,
			TEXT("AFLCombatCheats: OK SetRecoil (Pitch=%.2f Jitter=%.2f)"),
			Transient->RecoilPitchPerShot, Transient->RecoilYawJitterDegrees);
	}

	FAutoConsoleCommand GAFLCombatDamageCmd(
		TEXT("AFL.Combat.Damage"),
		TEXT("AFL-0105: apply GE_AFL_Damage_Pulse self-target. Usage: AFL.Combat.Damage [amount=18]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLCombatDamage));

	FAutoConsoleCommand GAFLCombatEnergyGainCmd(
		TEXT("AFL.Combat.EnergyGain"),
		TEXT("AFL-0105: apply GE_AFL_EnergyGain_Small (no-op until AFL-0701). Usage: AFL.Combat.EnergyGain [amount=10]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLCombatEnergyGain));

	FAutoConsoleCommand GAFLCombatGrantBeamCmd(
		TEXT("AFL.Combat.GrantBeam"),
		TEXT("AFL-0206: activate the player's UAFLAG_Laser_Beam channel (requires the AbilitySet to have granted the spec; full grant path lands in AFL-0214)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLCombatGrantBeam));

	FAutoConsoleCommand GAFLCombatHeatCmd(
		TEXT("AFL.Combat.Heat"),
		TEXT("AFL-0207: set Heat directly via GE_AFL_Heat_SetByCaller. Usage: AFL.Combat.Heat [amount=50]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLCombatHeat));

	FAutoConsoleCommand GAFLCombatForceOverheatCmd(
		TEXT("AFL.Combat.ForceOverheat"),
		TEXT("AFL-0207: set Heat = MaxHeat and apply State.Overheated."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLCombatForceOverheat));

	FAutoConsoleCommand GAFLCombatResetHeatCmd(
		TEXT("AFL.Combat.ResetHeat"),
		TEXT("AFL-0207: clear Heat and State.Overheated."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLCombatResetHeat));

	FAutoConsoleCommand GAFLCombatLoadTuningCmd(
		TEXT("AFL.Combat.LoadTuning"),
		TEXT("AFL-0209: swap UAFLAG_Laser_Pulse->TuningData live. Usage: AFL.Combat.LoadTuning <AssetPath e.g. /AFLCombat/Tuning/DA_AFLPulseTuning>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLCombatLoadTuning));

	FAutoConsoleCommand GAFLCombatSetSpreadCmd(
		TEXT("AFL.Combat.SetSpread"),
		TEXT("AFL-0209: tweak Pulse spread on a TRANSIENT copy (source DA untouched). Usage: AFL.Combat.SetSpread <baseDeg> <maxDeg> <perShotDeg>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLCombatSetSpread));

	FAutoConsoleCommand GAFLCombatSetRecoilCmd(
		TEXT("AFL.Combat.SetRecoil"),
		TEXT("AFL-0209: tweak Pulse recoil on a TRANSIENT copy (source DA untouched). Usage: AFL.Combat.SetRecoil <pitchPerShot> <yawJitter>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLCombatSetRecoil));

	// ─── BM-0105c lag-comp compensation proof: afl.LagComp.TestFire ──────────
	//
	// The isolated single-variable RTT-flip. Hand-aiming a moving target last
	// session produced uncontrolled samples (every shot's impact point differs);
	// this command removes the human from the aim loop entirely. It fires the
	// SHARED UAFLLagCompensationWorldSubsystem::ConfirmHit (the same code the
	// live Pulse path runs) at a FIXED, LATCHED world coordinate, varying only
	// the rewind delta via the afl.LagComp.ForceRTT CVar:
	//
	//   afl.LagComp.ForceRTT 0.2 ; afl.LagComp.TestFire
	//     -> latch C = dummy's position 0.2s ago; ConfirmHit(PC, 0.2, dummy, C)
	//        rewind ON -> box at past pose at C -> ACCEPT
	//   afl.LagComp.ForceRTT 0   ; afl.LagComp.TestFire replay
	//     -> reuse SAME C; ConfirmHit(PC, 0.0, dummy, C)
	//        rewind OFF -> box at current pose (~235cm from C) -> REJECT
	//
	// Identical coordinate, only RTT varies, verdict flips = the proof.

	// Latched fixed coordinate for the flip's "replay" leg. Static so the
	// second invocation reuses the exact coordinate the first latched.
	FVector GAFLLagCompLatchedCoord = FVector::ZeroVector;
	bool    GAFLLagCompLatched      = false;

	// Read the afl.LagComp.ForceRTT CVar (defined in AFLAG_Laser_Pulse.cpp) by
	// name — it registers globally, so cross-TU access is via the console
	// manager. Clamped to 0.2 exactly like the live path. -1 (real ping) maps
	// to 0 here because TestFire has no network ping to read; the operator is
	// expected to set ForceRTT explicitly for the flip.
	float ResolveForceRTTDelta()
	{
		float Raw = -1.0f;
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.LagComp.ForceRTT")))
		{
			Raw = CVar->GetFloat();
		}
		const float Effective = (Raw >= 0.0f) ? Raw : 0.0f;
		return FMath::Min(Effective, 0.2f);
	}

	// Find the test dummy + its history component in the first game world.
	// Returns the component (for SampleAtTime) and the owning actor (the
	// ConfirmHit target). nullptr if no dummy is placed / registered.
	UAFLPawnHitboxHistoryComponent* FindDummyHistory(AAFLLagTestDummy*& OutDummy)
	{
		OutDummy = nullptr;
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			for (TActorIterator<AAFLLagTestDummy> It(World); It; ++It)
			{
				if (UAFLPawnHitboxHistoryComponent* Hist = It->FindComponentByClass<UAFLPawnHitboxHistoryComponent>())
				{
					OutDummy = *It;
					return Hist;
				}
			}
		}
		return nullptr;
	}

	// Average the rewound bone locations into one representative point. This
	// mirrors how FAFLLagRewindToken::BuildBoundingBox derives its box (sum of
	// bone world locations) — the box CENTER is the natural "where the dummy
	// was" coordinate, so a shot at the average lands inside the rewound box.
	bool SampleDummyPastCenter(UAFLPawnHitboxHistoryComponent* Hist, UWorld* World, float PastDelta, FVector& OutCenter)
	{
		if (!Hist || !World)
		{
			return false;
		}
		const double SampleTime = static_cast<double>(World->GetTimeSeconds()) - static_cast<double>(PastDelta);
		TArray<FAFLHitboxBoneSample> Samples;
		if (!Hist->SampleAtTime(SampleTime, Samples) || Samples.Num() == 0)
		{
			return false;
		}
		FVector Sum = FVector::ZeroVector;
		for (const FAFLHitboxBoneSample& S : Samples)
		{
			Sum += S.WorldXForm.GetLocation();
		}
		OutCenter = Sum / static_cast<float>(Samples.Num());
		return true;
	}

	void HandleAFLLagCompTestFire(const TArray<FString>& Args)
	{
		const bool bReplay = Args.Num() > 0 && Args[0].Equals(TEXT("replay"), ESearchCase::IgnoreCase);

		AAFLLagTestDummy* Dummy = nullptr;
		UAFLPawnHitboxHistoryComponent* Hist = FindDummyHistory(Dummy);
		if (!Dummy || !Hist)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFLCombatCheats: FAIL TestFire — no AAFLLagTestDummy with a history component in any game world."));
			return;
		}

		UWorld* World = Dummy->GetWorld();
		UAFLLagCompensationWorldSubsystem* LagComp =
			World ? World->GetSubsystem<UAFLLagCompensationWorldSubsystem>() : nullptr;
		if (!LagComp)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFLCombatCheats: FAIL TestFire — no lag-comp subsystem."));
			return;
		}

		// The shooter PC to exclude from the rewind (its own pawn). The dummy
		// is not the shooter, so it stays in the rewind set regardless.
		APlayerController* PC = World->GetFirstPlayerController();

		const float Delta = ResolveForceRTTDelta();

		FVector Coord;
		if (bReplay)
		{
			if (!GAFLLagCompLatched)
			{
				UE_LOG(LogAFLCombat, Warning,
					TEXT("AFLCombatCheats: FAIL TestFire replay — no latched coordinate. Run 'afl.LagComp.TestFire' (no arg) first to latch."));
				return;
			}
			Coord = GAFLLagCompLatchedCoord;
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFLCombatCheats: TestFire REPLAY at latched C=(%.2f, %.2f, %.2f) delta=%.3f"),
				Coord.X, Coord.Y, Coord.Z, Delta);
		}
		else
		{
			// Latch the dummy's position 0.2s ago — the FIXED point both legs
			// of the flip fire at. 0.2 is the max-compensation window; latching
			// the past point (not "now") means the ForceRTT=0.2 leg accepts
			// (box rewinds onto C) and the ForceRTT=0 leg rejects (box at the
			// now-position, ~235cm from C at amplitude 400).
			if (!SampleDummyPastCenter(Hist, World, 0.2f, Coord))
			{
				UE_LOG(LogAFLCombat, Warning,
					TEXT("AFLCombatCheats: FAIL TestFire — history has no sample at now-0.2s yet (let the dummy tick a moment)."));
				return;
			}
			GAFLLagCompLatchedCoord = Coord;
			GAFLLagCompLatched      = true;

			FVector NowCenter = FVector::ZeroVector;
			SampleDummyPastCenter(Hist, World, 0.0f, NowCenter);
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFLCombatCheats: TestFire LATCH C=past_0.2s=(%.2f, %.2f, %.2f)  current=(%.2f, %.2f, %.2f)  delta=%.3f"),
				Coord.X, Coord.Y, Coord.Z, NowCenter.X, NowCenter.Y, NowCenter.Z, Delta);
		}

		// THE shared confirm path — identical to live Pulse. Emits the
		// "rewind dt=... entries=... verdict=..." line itself. S4 AFL-0408-FU-GUNFIRE added the
		// resolved-bone out-param; this RTT-flip proof ignores it (throwaway) -- it only asserts the
		// accept/reject verdict, which is UNCHANGED.
		FName ResolvedBoneUnused = NAME_None;
		const bool bAccept = LagComp->ConfirmHit(PC, Delta, Dummy, Coord, ResolvedBoneUnused);

		UE_LOG(LogAFLCombat, Display,
			TEXT("AFLCombatCheats: OK TestFire verdict=%s bone=%s (delta=%.3f, C=(%.2f, %.2f, %.2f))"),
			bAccept ? TEXT("ACCEPT") : TEXT("REJECT"), *ResolvedBoneUnused.ToString(), Delta, Coord.X, Coord.Y, Coord.Z);
	}

	FAutoConsoleCommand GAFLLagCompTestFireCmd(
		TEXT("afl.LagComp.TestFire"),
		TEXT("BM-0105c: fire the shared lag-comp ConfirmHit at the test dummy's latched 0.2s-ago position, using afl.LagComp.ForceRTT as the rewind delta. No arg = latch + fire; 'replay' = reuse latched coord. The flip: ForceRTT 0.2 + TestFire (ACCEPT), then ForceRTT 0 + TestFire replay (REJECT)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLLagCompTestFire));

	// ─── #43 selection-seam harness: afl.Cosmetic.SetEdge <color> ────────────────
	//
	// ─── afl.GroundTruth: possession + camera + CMC engine-state probe ──────────────
	//
	// Baseline diagnostic (P-CONTROLS reparent investigation). Dumps the engine's
	// ground truth so a "flying pawn" report can be judged on STATE, not visual guess.
	// One labeled [AFL_GROUND_TRUTH] line: PC, PC->GetPawn() + its class, the pawn's
	// own GetController() (round-trip), the player's ViewTarget, whether the debug
	// camera is active (PlayerCameraManager view target != pawn / DebugCameraController),
	// and the CMC's class (LyraCMC vs AFLCMC). World-context handler -> resolves the
	// PIE WINDOW's local PC. Read-only; touches nothing. Operator types it in console.
	void HandleAFLGroundTruth(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("[AFL_GROUND_TRUTH] no game world (run inside PIE)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC)
		{
			Ar.Log(TEXT("[AFL_GROUND_TRUTH] no PlayerController in this world."));
			return;
		}

		APawn* Pawn = PC->GetPawn();
		const FString PawnName = Pawn ? Pawn->GetName() : TEXT("NULL");
		const FString PawnClass = Pawn ? Pawn->GetClass()->GetName() : TEXT("NULL");

		// Round-trip: does the pawn point back at this controller? (true possession)
		AController* PawnController = Pawn ? Pawn->GetController() : nullptr;
		const FString PawnCtrlName = PawnController ? PawnController->GetName() : TEXT("NULL");
		const bool bRoundTripOk = (PawnController == PC);

		// ViewTarget + debug-camera signature.
		AActor* ViewTarget = PC->GetViewTarget();
		const FString ViewTargetName = ViewTarget ? ViewTarget->GetName() : TEXT("NULL");
		const bool bViewTargetIsPawn = (ViewTarget == Pawn);
		// Debug camera signature: ToggleDebugCamera spawns a UDebugCameraController and makes IT the
		// active player controller (so World->GetFirstPlayerController can BE the debug controller), or
		// the camera manager's view target diverges from the possessed pawn. Detect by class-name (no
		// hard dep on the engine debug-camera header) + the view-target divergence.
		const bool bDebugCam = PC->GetClass()->GetName().Contains(TEXT("DebugCamera"))
			|| (Pawn != nullptr && PC->PlayerCameraManager && PC->PlayerCameraManager->GetViewTarget() != Pawn);

		// CMC class (the reparent payoff signal: LyraCharacterMovementComponent vs AFLCharacterMovementComponent).
		FString CMCClass = TEXT("NULL");
		if (const ACharacter* Char = Cast<ACharacter>(Pawn))
		{
			if (const UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
			{
				CMCClass = CMC->GetClass()->GetName();
			}
		}

		const FString Tag = (Args.Num() > 0) ? Args[0] : TEXT("");

		Ar.Logf(TEXT("[AFL_GROUND_TRUTH] tag=%s | PC=%s | Pawn=%s (%s) | PawnController=%s roundTripOk=%s | ViewTarget=%s viewIsPawn=%s | DebugCam=%s | CMC=%s"),
			*Tag,
			*PC->GetName(),
			*PawnName, *PawnClass,
			*PawnCtrlName, bRoundTripOk ? TEXT("YES") : TEXT("no"),
			*ViewTargetName, bViewTargetIsPawn ? TEXT("YES") : TEXT("no"),
			bDebugCam ? TEXT("YES") : TEXT("no"),
			*CMCClass);

		// Also UE_LOG so it lands in the file log regardless of console echo.
		UE_LOG(LogAFLCombat, Log,
			TEXT("[AFL_GROUND_TRUTH] tag=%s PC=%s Pawn=%s(%s) roundTripOk=%s ViewTarget=%s viewIsPawn=%s DebugCam=%s CMC=%s"),
			*Tag, *PC->GetName(), *PawnName, *PawnClass, bRoundTripOk ? TEXT("YES") : TEXT("no"),
			*ViewTargetName, bViewTargetIsPawn ? TEXT("YES") : TEXT("no"),
			bDebugCam ? TEXT("YES") : TEXT("no"), *CMCClass);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLGroundTruthCmd(
		TEXT("afl.GroundTruth"),
		TEXT("Baseline diagnostic: dump [AFL_GROUND_TRUTH] engine state (PC, possessed pawn + class, controller round-trip, ViewTarget, debug-camera, CMC class). Optional arg = a label tag. Run in PIE console."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLGroundTruth));

	// WHY a console command (not just the UFUNCTION(Exec) SetCosmeticEdge): UFUNCTION(Exec) on a
	// CheatManagerExtension only routes when the cheat manager is active (Lyra gates it). The always-
	// available, world-context-aware FAutoConsoleCommandWithWorldArgsAndOutputDevice fires regardless --
	// and critically its handler receives the UWorld of the PIE WINDOW the command was typed in, so a
	// command typed in a CLIENT window resolves THAT client's PlayerController. That makes the Server RPC
	// take the genuine client->server hop (resolving "any world" could grab the server PC and no-op the hop).
	//
	// PURE CALLER, same contract as the exec: build FAFLCosmeticSelection from the current replicated
	// selection (don't clobber identity; seed AFL.Team.ARIA if unset so _Validate passes), set EdgeId,
	// hand to ServerSetCosmeticSelection. Nothing else. Server does all validation/gating/commit/replicate.
	void HandleAFLCosmeticSetEdge(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetEdge — usage: afl.Cosmetic.SetEdge <NeonPurple|NeonPink|NeonBlue|NeonGreen>"));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetEdge — no game world (run inside PIE)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetEdge — no UAFLCosmeticLoadoutComponent on the local player's PlayerState."));
			return;
		}

		FString IdStr = Args[0].TrimStartAndEnd();
		if (!IdStr.StartsWith(TEXT("AFL.Edge."), ESearchCase::IgnoreCase))
		{
			IdStr = FString::Printf(TEXT("AFL.Edge.%s"), *IdStr);
		}
		const FName EdgeId(*IdStr);

		FAFLCosmeticSelection Request = Loadout->GetSelection();
		if (Request.GetActiveIdentityId() == NAME_None)
		{
			Request.IdentityType = EAFLIdentityType::Team;
			Request.TeamId = FName(TEXT("AFL.Team.ARIA"));
		}
		Request.EdgeId = EdgeId;

		Loadout->ServerSetCosmeticSelection(Request); // PURE: client-issued; server does the rest.

		Ar.Logf(TEXT("afl.Cosmetic.SetEdge — client issued ServerSetCosmeticSelection(edge=%s). Watch [Loadout] RX/COMMIT/OnRep with `afl.SkinDiag 1`."),
			*EdgeId.ToString());
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetEdgeCmd(
		TEXT("afl.Cosmetic.SetEdge"),
		TEXT("#43 selection seam: client-issued PURE caller of ServerSetCosmeticSelection. Usage: afl.Cosmetic.SetEdge <NeonPurple|NeonPink|NeonBlue|NeonGreen> (or full AFL.Edge.<color>). NOT NeonRed (absent from BrandEdgeMap)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetEdge));

	// ─────────────────────────────────────────────────────────────────────────
	// afl.Cosmetic.RosterTest -- SELF-CYCLING roster QA harness (DeveloperTool).
	//
	// 28 identities x colours x lock x hit-flash is hundreds of manual commands. This drives the whole
	// matrix unattended on a timer so the operator only WATCHES for visual bugs. Every step emits a
	// structured AFL_TEST[...] marker so the log can be read AFTER PIE closes and turned into per-item
	// verdicts (never read mid-PIE -- MCP in a live PIE has crashed the editor).
	//
	// PHASES: 1 identity sweep (every brand's authored finish, held long enough to eyeball)
	//         2 colour sweep (every registry identity on the same body -- proves matrix distinctness)
	//         3 sponsor-lock assert (locked body must REFUSE, standard body must ACCEPT) -- automated PASS/FAIL
	//         4 hit-flash: writes HitPosition0 directly (see the note in the phase, below)
	// ─────────────────────────────────────────────────────────────────────────
	struct FAFLRosterTestState
	{
		TWeakObjectPtr<UWorld> World;
		TArray<TWeakObjectPtr<UAFLSkinColorAsset>> Finishes;   // phase 1 payloads
		TArray<FName> FinishLabels;
		TArray<FGameplayTag> ColourTags;                        // phase 2 payloads
		int32 Phase = 0;
		int32 Index = 0;
		float HoldSeconds = 2.5f;
		FTimerHandle Timer;
		bool bActive = false;
		// FIX 1: the possessed body's brand, resolved once at START (empty tag == standard body).
		FGameplayTag LockedBrandTag;
		FLinearColor LockedBrandTone = FLinearColor::Black;
		// FIX 2: phase-2 writes MIDs RAW, so it must restore what it clobbered before phase 3 asserts.
		TWeakObjectPtr<UAFLSkinColorAsset> RestorePreset;
		// PART-LOSS TOLERANCE: consecutive zero-part reads (death/dismember detaches the part actors).
		int32 MissStreak = 0;
	};
	static FAFLRosterTestState GRosterTest;

	static FString AFLRT_Hex(const FLinearColor& C)
	{
		const FColor S = C.ToFColor(true);
		return FString::Printf(TEXT("#%02X%02X%02X"), S.R, S.G, S.B);
	}

	// Every AAFLCharacterPartActor hanging off the local pawn (the robot body parts).
	static void AFLRT_GatherParts(UWorld* World, TArray<AAFLCharacterPartActor*>& Out)
	{
		Out.Reset();
		if (!World) { return; }
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn) { return; }
		TArray<AActor*> Attached;
		Pawn->GetAttachedActors(Attached);
		for (AActor* A : Attached)
		{
			if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(A)) { Out.Add(Part); }
		}
		for (UChildActorComponent* CAC : TInlineComponentArray<UChildActorComponent*>(Pawn))
		{
			if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC->GetChildActor()))
			{
				Out.AddUnique(Part);
			}
		}
	}

	// Read back what actually landed on the live MID -- the render truth, not the intent.
	static bool AFLRT_ReadLiveEmissive(UWorld* World, FLinearColor& Out)
	{
		TArray<AAFLCharacterPartActor*> Parts;
		AFLRT_GatherParts(World, Parts);
		for (AAFLCharacterPartActor* Part : Parts)
		{
			TArray<UMeshComponent*> Meshes;
			Part->GetComponents<UMeshComponent>(Meshes);
			for (UMeshComponent* M : Meshes)
			{
				if (!M || M->GetNumMaterials() <= 0) { continue; }
				if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
				{
					if (MID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("EmissiveColor")), Out)) { return true; }
				}
			}
		}
		return false;
	}

	static void AFLRT_Step();

	static void AFLRT_Schedule(float Delay)
	{
		if (UWorld* W = GRosterTest.World.Get())
		{
			W->GetTimerManager().SetTimer(GRosterTest.Timer, FTimerDelegate::CreateStatic(&AFLRT_Step), Delay, false);
		}
	}

	static void AFLRT_Finish(const TCHAR* Why)
	{
		GRosterTest.bActive = false;
		if (UWorld* W = GRosterTest.World.Get()) { W->GetTimerManager().ClearTimer(GRosterTest.Timer); }
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Run=END reason=%s]"), Why);
	}

	static void AFLRT_Step()
	{
		UWorld* World = GRosterTest.World.Get();
		if (!World || !GRosterTest.bActive) { AFLRT_Finish(TEXT("world-gone")); return; }

		// PART-LOSS TOLERANCE: a mid-run DEATH/dismemberment detaches the part actors, so a zero-part read is
		// transient, not fatal. The first run lost phases 2-4 (the lock + hit-flash verdicts) because the
		// operator was killed during the sweep. Skip the step and retry; only give up after a sustained gap,
		// which is the genuine "pawn is gone for good" case.
		TArray<AAFLCharacterPartActor*> Parts;
		AFLRT_GatherParts(World, Parts);
		if (Parts.Num() == 0)
		{
			++GRosterTest.MissStreak;
			if (GRosterTest.MissStreak >= 20)   // ~20s at the 1s retry cadence
			{
				AFLRT_Finish(TEXT("no-part-actors-on-pawn-sustained"));
				return;
			}
			if (GRosterTest.MissStreak == 1)
			{
				UE_LOG(LogAFLCombat, Warning,
					TEXT("AFL_TEST[Warn=PARTS-LOST] no part actors (death/dismember?) -- holding phase %d step %d, retrying"),
					GRosterTest.Phase, GRosterTest.Index);
			}
			AFLRT_Schedule(1.0f);
			return;
		}
		if (GRosterTest.MissStreak > 0)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[PartsRecovered afterMisses=%d phase=%d step=%d]"),
				GRosterTest.MissStreak, GRosterTest.Phase, GRosterTest.Index);
			GRosterTest.MissStreak = 0;
		}

		// ---------- PHASE 1: identity sweep ----------
		if (GRosterTest.Phase == 1)
		{
			if (!GRosterTest.Finishes.IsValidIndex(GRosterTest.Index))
			{
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Phase=1 IdentitySweep DONE count=%d]"), GRosterTest.Finishes.Num());
				GRosterTest.Phase = 2; GRosterTest.Index = 0; AFLRT_Schedule(0.5f); return;
			}
			UAFLSkinColorAsset* Preset = GRosterTest.Finishes[GRosterTest.Index].Get();
			const FName Label = GRosterTest.FinishLabels.IsValidIndex(GRosterTest.Index)
				? GRosterTest.FinishLabels[GRosterTest.Index] : NAME_None;
			if (Preset)
			{
				for (AAFLCharacterPartActor* Part : Parts) { Part->ApplySkinColor(Preset); }
				FLinearColor Live;
				const bool bRead = AFLRT_ReadLiveEmissive(World, Live);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[Identity=%s preset=%s live=%s step=%d/%d]"),
					*Label.ToString(), *Preset->GetName(),
					bRead ? *AFLRT_Hex(Live) : TEXT("<unread>"),
					GRosterTest.Index + 1, GRosterTest.Finishes.Num());
			}
			++GRosterTest.Index;
			AFLRT_Schedule(GRosterTest.HoldSeconds);
			return;
		}

		// ---------- PHASE 2: colour sweep (matrix distinctness, same body) ----------
		if (GRosterTest.Phase == 2)
		{
			if (!GRosterTest.ColourTags.IsValidIndex(GRosterTest.Index))
			{
				// FIX 2: this phase wrote MIDs DIRECTLY (bypassing resolve + lock, by design -- it isolates
				// render). That leaves the body wearing the last swept colour, which then poisons phase 3's
				// "before" reading. Re-apply a real preset through the normal path so phase 3 starts from a
				// legitimately-resolved state instead of raw leftovers.
				if (UAFLSkinColorAsset* Restore = GRosterTest.RestorePreset.Get())
				{
					for (AAFLCharacterPartActor* Part : Parts) { Part->ApplySkinColor(Restore); }
					FLinearColor Settled;
					const bool bRead = AFLRT_ReadLiveEmissive(World, Settled);
					UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Phase=2 RESTORE preset=%s settled=%s]"),
						*Restore->GetName(), bRead ? *AFLRT_Hex(Settled) : TEXT("<unread>"));
				}
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Phase=2 ColourSweep DONE count=%d]"), GRosterTest.ColourTags.Num());
				GRosterTest.Phase = 3; GRosterTest.Index = 0; AFLRT_Schedule(0.5f); return;
			}
			const FGameplayTag Tag = GRosterTest.ColourTags[GRosterTest.Index];
			FAFLColorIdentity Ident;
			if (UAFLCosmeticCatalogSubsystem::ResolveColorIdentity(World, Tag, Ident))
			{
				// Drive the MIDs straight from the registry tone -- isolates RESOLVE+RENDER from entitlement.
				for (AAFLCharacterPartActor* Part : Parts)
				{
					TArray<UMeshComponent*> Meshes;
					Part->GetComponents<UMeshComponent>(Meshes);
					for (UMeshComponent* M : Meshes)
					{
						if (!M) { continue; }
						for (int32 S = 0; S < M->GetNumMaterials(); ++S)
						{
							if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(S)))
							{
								MID->SetVectorParameterValue(TEXT("EmissiveColor"), Ident.SkinFinish.EmissiveColor1);
								MID->SetVectorParameterValue(TEXT("EdgeGlowColor"), Ident.SkinFinish.EdgeGlowColor);
								MID->SetVectorParameterValue(TEXT("TeamColor"), Ident.SkinFinish.TeamColor);
							}
						}
					}
				}
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Colour=%s hex=%s edge=%s step=%d/%d]"),
					*Tag.ToString(), *AFLRT_Hex(Ident.SkinFinish.EmissiveColor1),
					*AFLRT_Hex(Ident.SkinFinish.EdgeGlowColor),
					GRosterTest.Index + 1, GRosterTest.ColourTags.Num());
			}
			else
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[Colour=%s UNRESOLVED]"), *Tag.ToString());
			}
			++GRosterTest.Index;
			AFLRT_Schedule(GRosterTest.HoldSeconds);
			return;
		}

		// ---------- PHASE 3: sponsor-lock assert (automated PASS/FAIL, not eyeball) ----------
		if (GRosterTest.Phase == 3)
		{
			// Find a NON-brand preset to push, then check whether the tone actually moved.
			UAFLSkinColorAsset* Probe = nullptr;
			for (int32 i = 0; i < GRosterTest.Finishes.Num(); ++i)
			{
				if (GRosterTest.FinishLabels.IsValidIndex(i) && GRosterTest.FinishLabels[i] != FName(TEXT("FANATICS")))
				{
					Probe = GRosterTest.Finishes[i].Get();
					if (Probe) { break; }
				}
			}
			// FIX 3: the ORIGINAL assert was "a locked body must not CHANGE" -- wrong, and it produced a false
			// FAIL. A locked body legitimately changes: it snaps to the BRAND tone. The real question is WHICH
			// colour it landed on. Locked => must equal the body's own brand tone (the probe is refused);
			// standard => must equal the PROBE's resolved tone (the probe is accepted). Locked-ness now comes
			// from the registry via the body's own tag, not from a class-name substring match.
			const bool bLockedBody = GRosterTest.LockedBrandTag.IsValid();
			FLinearColor ProbeTone(ForceInit);
			bool bProbeToneKnown = false;
			if (Probe && Probe->GetColorIdentityTag().IsValid())
			{
				FAFLColorIdentity ProbeId;
				if (UAFLCosmeticCatalogSubsystem::ResolveColorIdentity(World, Probe->GetColorIdentityTag(), ProbeId))
				{
					ProbeTone = ProbeId.SkinFinish.EmissiveColor1; bProbeToneKnown = true;
				}
			}
			if (Probe && !bProbeToneKnown)
			{
				// Untagged probe -> its BAKED EmissiveColor is what a standard body should adopt.
				if (const FLinearColor* Baked = Probe->GetColors().Find(FName(TEXT("EmissiveColor"))))
				{
					ProbeTone = *Baked; bProbeToneKnown = true;
				}
			}

			FLinearColor Before(ForceInit), After(ForceInit);
			AFLRT_ReadLiveEmissive(World, Before);
			for (AAFLCharacterPartActor* Part : Parts) { if (Probe) { Part->ApplySkinColor(Probe); } }
			AFLRT_ReadLiveEmissive(World, After);

			const FLinearColor& Expected = bLockedBody ? GRosterTest.LockedBrandTone : ProbeTone;
			const bool bPass = (bLockedBody || bProbeToneKnown) && After.Equals(Expected, 0.02f);
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[Lock=%s mode=%s probe=%s before=%s after=%s expectedTone=%s]"),
				bPass ? TEXT("PASS") : TEXT("FAIL"),
				bLockedBody ? TEXT("LOCKED-must-hold-brand") : TEXT("STANDARD-must-adopt-probe"),
				Probe ? *Probe->GetName() : TEXT("<none>"),
				*AFLRT_Hex(Before), *AFLRT_Hex(After), *AFLRT_Hex(Expected));
			GRosterTest.Phase = 4; GRosterTest.Index = 0; AFLRT_Schedule(1.0f);
			return;
		}

		// ---------- PHASE 4: hit-flash ----------
		// NOTE (measured 2026-07-25): NOTHING in C++ or content writes HitPosition0 -- the material exposes
		// the params but no runtime driver exists, which is why hit-flash has never fired. So this phase
		// writes HitPosition0 DIRECTLY. That makes it a clean isolation: if the body flashes here, the
		// material chain is alive and only the gameplay driver is missing; if it does not, the chain itself
		// is dead. Either way the operator gets a definitive answer instead of an ambiguity.
		if (GRosterTest.Phase == 4)
		{
			if (GRosterTest.Index >= 3) { AFLRT_Finish(TEXT("complete")); return; }
			APlayerController* PC = World->GetFirstPlayerController();
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			const FVector HitPos = Pawn ? Pawn->GetActorLocation() + FVector(0.f, 0.f, 40.f) : FVector::ZeroVector;
			// FIX 4: DETERMINISTIC hit-flash verdict instead of "did you see a flash?". SetVectorParameterValue
			// on a param the material does NOT expose is a SILENT no-op -- so we write, then READ BACK. If the
			// readback fails, the param is absent and the chain is DEAD (nothing could ever flash). If it
			// reads back, the material accepted it and only the gameplay driver is missing (nothing in C++ or
			// content writes HitPosition0 -- verified). Either way the log answers it without eyeballs.
			int32 Written = 0, ParamPresent = 0, ParamAbsent = 0;
			for (AAFLCharacterPartActor* Part : Parts)
			{
				TArray<UMeshComponent*> Meshes;
				Part->GetComponents<UMeshComponent>(Meshes);
				for (UMeshComponent* M : Meshes)
				{
					if (!M) { continue; }
					for (int32 S = 0; S < M->GetNumMaterials(); ++S)
					{
						if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(S)))
						{
							MID->SetVectorParameterValue(TEXT("HitPosition0"), FLinearColor(HitPos));
							MID->SetScalarParameterValue(TEXT("HitFlashStrength"), 5.f);
							MID->SetScalarParameterValue(TEXT("HitFlashRadius"), 60.f);
							++Written;
							FLinearColor Echo(ForceInit);
							if (MID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("HitPosition0")), Echo))
							{
								++ParamPresent;
							}
							else
							{
								++ParamAbsent;
							}
						}
					}
				}
			}
			const TCHAR* Chain = (ParamPresent > 0) ? TEXT("MATERIAL-ALIVE-driver-missing") : TEXT("MATERIAL-DEAD-param-absent");
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[HitFlash=fired pulse=%d/3 pos=%s midsWritten=%d paramPresent=%d paramAbsent=%d verdict=%s]"),
				GRosterTest.Index + 1, *HitPos.ToCompactString(), Written, ParamPresent, ParamAbsent, Chain);
			++GRosterTest.Index;
			AFLRT_Schedule(1.5f);
			return;
		}

		AFLRT_Finish(TEXT("unknown-phase"));
	}

	void HandleAFLRosterTest(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.RosterTest - run inside PIE."));
			return;
		}
		if (Args.Num() >= 1 && Args[0].Equals(TEXT("stop"), ESearchCase::IgnoreCase))
		{
			AFLRT_Finish(TEXT("operator-stop"));
			Ar.Log(TEXT("afl.Cosmetic.RosterTest - stopped."));
			return;
		}

		GRosterTest = FAFLRosterTestState();
		GRosterTest.World = World;
		if (Args.Num() >= 1)
		{
			const float Hold = FCString::Atof(*Args[0]);
			if (Hold > 0.1f) { GRosterTest.HoldSeconds = Hold; }
		}

		// PHASE-1 payload: every brand's AUTHORED finish, straight off the shipped BrandEdgeMap.
		if (UAFLBrandEdgeMap* Map = LoadObject<UAFLBrandEdgeMap>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/SkinColors/DA_AFL_BrandEdgeMap.DA_AFL_BrandEdgeMap")))
		{
			for (const TPair<FGameplayTag, TObjectPtr<UAFLSkinColorAsset>>& KV : Map->BrandToEdge)
			{
				if (UAFLSkinColorAsset* Preset = KV.Value.Get())
				{
					GRosterTest.Finishes.Add(Preset);
					FString Short = KV.Key.ToString();
					Short.RemoveFromStart(TEXT("Cosmetic.Brand."));
					GRosterTest.FinishLabels.Add(FName(*Short));
					// FIX 2 payload: first authored finish doubles as the phase-2 restore preset, so the
					// raw MID writes get replaced by a properly-resolved state before phase 3 asserts.
					if (!GRosterTest.RestorePreset.IsValid()) { GRosterTest.RestorePreset = Preset; }
				}
			}
		}
		// PHASE-2 payload: every registry identity. Loaded BY PATH (same pattern as the brand map above) --
		// the catalog subsystem exposes no public registry accessor, only the per-tag ResolveColorIdentity.
		if (const UAFLColorIdentityRegistry* Reg = LoadObject<UAFLColorIdentityRegistry>(nullptr,
			TEXT("/Game/BagMan/Cosmetics/DA_AFL_ColorIdentityRegistry.DA_AFL_ColorIdentityRegistry")))
		{
			for (const FAFLColorIdentity& Id : Reg->Identities)
			{
				if (Id.IdentityTag.IsValid()) { GRosterTest.ColourTags.Add(Id.IdentityTag); }
			}
		}

		GRosterTest.Phase = 1;
		GRosterTest.Index = 0;
		GRosterTest.bActive = true;

		// FIX 1: report WHICH body we are sweeping, and shout if it is LOCKED. On a sponsor body every
		// TAGGED preset is legitimately overridden to the brand tone, so the identity sweep becomes a lock
		// test, not a colour test -- the first run looked green because 24 of 28 presets are untagged and
		// bypassed the lock. Surfacing this up-front stops an invalid run being read as roster QA.
		FString BodyName(TEXT("<none>"));
		bool bLockedRun = false;
		{
			TArray<AAFLCharacterPartActor*> Parts0;
			AFLRT_GatherParts(World, Parts0);
			for (AAFLCharacterPartActor* Part : Parts0)
			{
				BodyName = Part->GetClass()->GetName();
				const FGameplayTag BrandTag = Part->GetBrandColorIdentityTag();
				FAFLColorIdentity BrandId;
				if (BrandTag.IsValid()
					&& UAFLCosmeticCatalogSubsystem::ResolveColorIdentity(World, BrandTag, BrandId)
					&& BrandId.bColorLocked)
				{
					bLockedRun = true;
					GRosterTest.LockedBrandTag = BrandTag;
					GRosterTest.LockedBrandTone = BrandId.SkinFinish.EmissiveColor1;
				}
				break;
			}
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Body=%s locked=%s brand=%s]"),
			*BodyName, bLockedRun ? TEXT("YES") : TEXT("no"),
			GRosterTest.LockedBrandTag.IsValid() ? *GRosterTest.LockedBrandTag.ToString() : TEXT("<none>"));
		if (bLockedRun)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[Warn=LOCKED-BODY] identity sweep on a SPONSOR body -- every TAGGED preset will correctly render the brand tone. This run tests the LOCK, not the roster. Possess a STANDARD body for roster colour QA."));
			Ar.Log(TEXT("WARNING: possessed body is a LOCKED sponsor -- identity sweep will show brand colour for tagged presets. Use a standard body for roster QA."));
		}

		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[Run=START identities=%d colours=%d hold=%.1fs] -- watch for: wrong colour, broken piping, missing/wrong emblem, visor glitch, mesh tear"),
			GRosterTest.Finishes.Num(), GRosterTest.ColourTags.Num(), GRosterTest.HoldSeconds);
		Ar.Logf(TEXT("afl.Cosmetic.RosterTest - started: %d identities then %d colours at %.1fs each, then lock-assert + hit-flash. Watch the pawn; AIK reads the log after you close PIE."),
			GRosterTest.Finishes.Num(), GRosterTest.ColourTags.Num(), GRosterTest.HoldSeconds);
		AFLRT_Schedule(0.5f);
	}

	// ─────────────────────────────────────────────────────────────────────────
	// afl.Cosmetic.PossessAs <BRAND> -- DEV body swap (DeveloperTool).
	//
	// WHY: the X bodies exist on disk but are unreachable in PIE -- FANATICS_X is store-available but LOCKED
	// (useless for standard QA) and ARIA_X is not in the store at all. So every "standard" run has landed on a
	// STOCK original, and the unique-body systems (piping guard, unique-body gib slot, X render) have NEVER
	// been QA'd, because they only exist where bUniqueBodyUVs == true.
	//
	// This bypasses store + entitlement + loadout entirely and swaps the body part directly, mirroring
	// UAFLCharacterPartSelectorComponent::AddBody: Lyra's CharacterParts classes carry no LYRAGAME_API export,
	// but AddCharacterPart / RemoveAllCharacterParts ARE UFUNCTIONs, so the reflected thunk is callable.
	// REMOVE-then-ADD, because AddCharacterPart APPENDS -- a plain add would stack overlapping robots.
	// STICKY POSSESS: a body swap does NOT survive death -- the respawned pawn gets the DEFAULT body again.
	// That silently confounded three QA runs: the operator possessed ARIA_X, died mid-test, respawned as the
	// STOCK ARIA, and the subsequent dismember was read as an "ARIA_X gib defect" that did not exist. Rather
	// than merely warn (a warning still costs the run), remember the requested brand and RE-APPLY it whenever
	// the live body drifts. Polled rather than delegate-bound: no binding lifetime to get wrong in dev code,
	// and a 1s tick is free at this scale. afl.Cosmetic.PossessAs off  clears it.
	struct FAFLStickyPossess
	{
		TWeakObjectPtr<UWorld> World;
		FString Brand;
		bool bForceBase = false;
		TWeakObjectPtr<UClass> WantClass;
		TStrongObjectPtr<UClass> KeepAlive;   // hardening: a cheat-loaded BP class has no other hard referencer until the part actor spawns; hold a strong ref so sticky can't silently die on GC (reset when sticky clears)
		FTimerHandle Timer;
		int32 ReapplyCount = 0;
	};
	static FAFLStickyPossess GStickyPossess;

	static bool AFLPossessAs_Apply(UWorld* World, UClass* PartClass, FString& OutError);

	static void AFLStickyPossess_Tick()
	{
		UWorld* World = GStickyPossess.World.Get();
		UClass* Want = GStickyPossess.WantClass.Get();
		if (!World || !Want) { return; }

		// Is the live body already the one we asked for? Cheap check on the pawn's part actors.
		TArray<AAFLCharacterPartActor*> Parts;
		AFLRT_GatherParts(World, Parts);
		bool bCorrect = false;
		for (AAFLCharacterPartActor* Part : Parts)
		{
			if (Part && Part->GetClass() == Want) { bCorrect = true; break; }
		}
		if (!bCorrect && Parts.Num() > 0)
		{
			FString Err;
			if (AFLPossessAs_Apply(World, Want, Err))
			{
				++GStickyPossess.ReapplyCount;
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[PossessAs STICKY-REAPPLY brand=%s class=%s count=%d] (body had reverted -- likely respawn)"),
					*GStickyPossess.Brand, *GetNameSafe(Want), GStickyPossess.ReapplyCount);
			}
		}
	}

	// Shared swap: find the stock CharacterParts controller component and REMOVE-then-ADD the body.
	// ⚠ MUST walk the class SUPER-CHAIN: the component is a BP SUBCLASS (B_BagMan_AssignCharacterPart_C),
	// so testing only the LEAF class name misses it every time -- exactly how the first cut of this cheat
	// silently no-op'd and left the stock body in place. Mirrors UAFLCharacterPartSelectorComponent::AddBody,
	// which documents the same trap. Shared so the sticky re-apply uses the identical path.
	static bool AFLPossessAs_Apply(UWorld* World, UClass* PartClass, FString& OutError)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->GetPawn()) { OutError = TEXT("no possessed pawn."); return false; }

		UActorComponent* StockPartsComp = nullptr;
		TInlineComponentArray<UActorComponent*> Comps(PC);
		for (UActorComponent* Comp : Comps)
		{
			if (!Comp) { continue; }
			for (const UClass* C = Comp->GetClass(); C; C = C->GetSuperClass())
			{
				if (C->GetName().Contains(TEXT("LyraControllerComponent_CharacterParts")))
				{
					StockPartsComp = Comp;
					break;
				}
			}
			if (StockPartsComp) { break; }
		}
		UFunction* AddFn = StockPartsComp ? StockPartsComp->FindFunction(FName(TEXT("AddCharacterPart"))) : nullptr;
		UFunction* RemoveAllFn = StockPartsComp ? StockPartsComp->FindFunction(FName(TEXT("RemoveAllCharacterParts"))) : nullptr;
		if (!StockPartsComp || !AddFn)
		{
			OutError = TEXT("stock CharacterParts component / AddCharacterPart not found on the controller.");
			return false;
		}

		if (RemoveAllFn) { StockPartsComp->ProcessEvent(RemoveAllFn, nullptr); }
		struct FAddCharacterPartArgs { FLyraCharacterPart NewPart; };
		FAddCharacterPartArgs AddArgs;
		AddArgs.NewPart.PartClass = PartClass;
		AddArgs.NewPart.SocketName = NAME_None;
		AddArgs.NewPart.CollisionMode = ECharacterCustomizationCollisionMode::NoCollision;
		StockPartsComp->ProcessEvent(AddFn, &AddArgs);
		return true;
	}

	void HandleAFLPossessAs(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.PossessAs - run inside PIE."));
			return;
		}
		if (Args.Num() >= 1 && Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase))
		{
			if (UWorld* W = GStickyPossess.World.Get()) { W->GetTimerManager().ClearTimer(GStickyPossess.Timer); }
			GStickyPossess = FAFLStickyPossess();
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[PossessAs sticky=OFF]"));
			Ar.Log(TEXT("afl.Cosmetic.PossessAs - sticky cleared; the body will follow normal selection again."));
			return;
		}
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.PossessAs <BRAND>  e.g. 'ARIA' -> B_AFL_Robot_ARIA_X (falls back to B_AFL_Robot_ARIA). Append ':base' to force the stock original. OR a full path e.g. '/Game/BagMan/Characters/ProMod_M01/B_AFL_ProMod_M01' (package-only path auto-appends '.<Leaf>_C')."));
			return;
		}

		FString Arg = Args[0].TrimStartAndEnd();

		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!PC || !Pawn)
		{
			Ar.Log(TEXT("afl.Cosmetic.PossessAs - no possessed pawn."));
			return;
		}

		UClass* PartClass = nullptr;
		FString Brand = Arg;          // sticky/log display token (the full path on the PATH branch)
		bool bForceBase = false;

		if (Arg.StartsWith(TEXT("/")))
		{
			// PATH branch: a full object/class path (ProMod part BPs live outside the hardcoded robot
			// folder/prefix). Load directly; skip the brand -> _X/base construction entirely.
			// ':base' is BRAND-only -- it is NEVER chopped here, so a path is passed through verbatim.
			FString PathStr = Arg;
			if (!Arg.Contains(TEXT(".")))
			{
				// package-only path -> append ".<LeafName>_C"
				FString Leaf;
				if (Arg.Split(TEXT("/"), nullptr, &Leaf, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
				{
					PathStr = FString::Printf(TEXT("%s.%s_C"), *Arg, *Leaf);
				}
			}
			PartClass = LoadObject<UClass>(nullptr, *PathStr);
			if (!PartClass)
			{
				Ar.Logf(TEXT("afl.Cosmetic.PossessAs - no class at '%s'."), *PathStr);
				return;
			}
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[PossessAs resolve brand=%s variant=PATH class=%s]"),
				*Arg, *GetNameSafe(PartClass));
			Ar.Logf(TEXT("afl.Cosmetic.PossessAs - resolved PATH class: %s"), *GetNameSafe(PartClass));
		}
		else
		{
			// BRAND branch (UNCHANGED behaviour): 'ARIA' -> _X first, base fallback; ':base' forces stock.
			if (Brand.EndsWith(TEXT(":base"), ESearchCase::IgnoreCase))
			{
				Brand.LeftChopInline(5); bForceBase = true;
			}
			// Prefer the X body (unique-body: that is the whole point), fall back to the stock original.
			const FString Root = TEXT("/Game/BagMan/Characters/Cosmetics/B_AFL_Robot_");
			FString Chosen;
			if (!bForceBase)
			{
				Chosen = FString::Printf(TEXT("%s%s_X.B_AFL_Robot_%s_X_C"), *Root, *Brand, *Brand);
				PartClass = LoadObject<UClass>(nullptr, *Chosen);
			}
			if (!PartClass)
			{
				Chosen = FString::Printf(TEXT("%s%s.B_AFL_Robot_%s_C"), *Root, *Brand, *Brand);
				PartClass = LoadObject<UClass>(nullptr, *Chosen);
			}
			if (!PartClass)
			{
				Ar.Logf(TEXT("afl.Cosmetic.PossessAs - no body found for '%s' (tried _X then base)."), *Brand);
				return;
			}
			// Announce the RESOLVED class before touching anything: if the _X body is missing and this silently
			// fell back to the stock original, that must be visible in the log AND on the console, not inferred
			// afterwards from "the wrong robot showed up".
			const bool bIsXBody = Chosen.Contains(TEXT("_X."));
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[PossessAs resolve brand=%s variant=%s class=%s]"),
				*Brand, bIsXBody ? TEXT("X") : TEXT("BASE"), *GetNameSafe(PartClass));
			Ar.Logf(TEXT("afl.Cosmetic.PossessAs - resolved %s body: %s"),
				bIsXBody ? TEXT("X") : TEXT("BASE (no _X found!)"), *GetNameSafe(PartClass));
		}

		FString Err;
		if (!AFLPossessAs_Apply(World, PartClass, Err))
		{
			Ar.Logf(TEXT("afl.Cosmetic.PossessAs - %s"), *Err);
			return;
		}

		// Arm STICKY re-apply so a death/respawn cannot silently revert the body mid-QA.
		GStickyPossess.World = World;
		GStickyPossess.Brand = Brand;            // display only (full path on the PATH branch)
		GStickyPossess.bForceBase = bForceBase;  // always false on the PATH branch
		GStickyPossess.WantClass = PartClass;
		GStickyPossess.KeepAlive.Reset(PartClass); // hardening: hold a strong ref so the cheat-loaded BP class can't be GC'd out from under sticky
		GStickyPossess.ReapplyCount = 0;
		World->GetTimerManager().SetTimer(GStickyPossess.Timer,
			FTimerDelegate::CreateStatic(&AFLStickyPossess_Tick), 1.0f, true);

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[PossessAs brand=%s class=%s sticky=ON]"), *Brand, *GetNameSafe(PartClass));
		Ar.Logf(TEXT("afl.Cosmetic.PossessAs - body swapped to %s (STICKY: re-applied on respawn; 'afl.Cosmetic.PossessAs off' to clear). AFL_TEST[Body=...] confirms which body and whether it is locked."),
			*GetNameSafe(PartClass));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLPossessAsCmd(
		TEXT("afl.Cosmetic.PossessAs"),
		TEXT("DEV body swap, bypasses store+entitlement+loadout. Usage: afl.Cosmetic.PossessAs <BRAND | /Full/Path> | off. BRAND (e.g. 'ARIA') prefers B_AFL_Robot_<BRAND>_X, falls back to stock; append ':base' to force stock. A leading '/' is a full class path (e.g. /Game/BagMan/Characters/ProMod_M01/B_AFL_ProMod_M01) -- package-only paths auto-append '.<Leaf>_C'; ':base' is brand-only and never chops a path. Reaches ProMod part BPs outside the robot folder/prefix."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLPossessAs));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLRosterTestCmd(
		TEXT("afl.Cosmetic.RosterTest"),
		TEXT("SELF-CYCLING roster QA. Usage: afl.Cosmetic.RosterTest [holdSeconds] | stop. Sweeps every brand identity, then every registry colour, then asserts the sponsor lock, then fires hit-flash. Emits AFL_TEST[...] markers for post-PIE log verdicts."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLRosterTest));

	// --- BODY selection seam: afl.Cosmetic.SetBody <color> (Option B body axis) ---
	// MIRRORS HandleAFLCosmeticSetEdge EXACTLY, but sets BodyId (AFL.Body.<color> -> a Finish preset via the
	// catalog) instead of EdgeId. The two axes are INDEPENDENT -> mix-and-match (purple body x green edge). PURE
	// caller: read the current replicated selection, seed AFL.Team.ARIA if identity unset (so _Validate passes),
	// set ONLY BodyId, hand to ServerSetCosmeticSelection. Server does all validation/gating/commit/replicate.
	void HandleAFLCosmeticSetBody(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetBody - usage: afl.Cosmetic.SetBody <NeonBlue|NeonPurple|...|Lime> (or full AFL.Body.<color>)."));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetBody - no game world (run inside PIE)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetBody - no UAFLCosmeticLoadoutComponent on the local player's PlayerState."));
			return;
		}

		FString IdStr = Args[0].TrimStartAndEnd();
		if (!IdStr.StartsWith(TEXT("AFL.Body."), ESearchCase::IgnoreCase))
		{
			IdStr = FString::Printf(TEXT("AFL.Body.%s"), *IdStr);
		}
		const FName BodyId(*IdStr);

		FAFLCosmeticSelection Request = Loadout->GetSelection();
		if (Request.GetActiveIdentityId() == NAME_None)
		{
			Request.IdentityType = EAFLIdentityType::Team;
			Request.TeamId = FName(TEXT("AFL.Team.ARIA"));
		}
		Request.BodyId = BodyId;

		Loadout->ServerSetCosmeticSelection(Request); // PURE: client-issued; server does the rest.

		Ar.Logf(TEXT("afl.Cosmetic.SetBody - client issued ServerSetCosmeticSelection(body=%s). Watch [Loadout] RX/COMMIT/OnRep with `afl.SkinDiag 1`."),
			*BodyId.ToString());
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetBodyCmd(
		TEXT("afl.Cosmetic.SetBody"),
		TEXT("Option B body axis: client-issued PURE caller of ServerSetCosmeticSelection (sets BodyId -> a Finish preset). Independent of SetEdge -> mix-and-match. Usage: afl.Cosmetic.SetBody <color> (or full AFL.Body.<color>)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetBody));

	// --- BEAM selection seam: afl.Cosmetic.SetBeam <color> (INDEPENDENT BeamId axis) ---
	// MIRRORS HandleAFLCosmeticSetBody EXACTLY, but sets BeamId (AFL.Beam.<color> -> a beam-color SKU via the
	// catalog) instead of BodyId. The beam is its OWN owned item, INDEPENDENT of the weapon AND its skin -> a
	// blue weapon can fire a red beam (mix-and-match). PURE caller: read the current replicated selection, seed
	// AFL.Team.ARIA if identity unset (so _Validate passes), set ONLY BeamId, hand to ServerSetCosmeticSelection.
	// The BeamId consumer (RefreshBeamColorForPawn -> SetBeamColor -> OnRep_BeamColor -> ApplyBeamColorToEquipped)
	// then reflection-writes LaserTintColor on the equipped weapon instance; the beam re-reads it on the next fire.
	void HandleAFLCosmeticSetBeam(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetBeam - usage: afl.Cosmetic.SetBeam <CrimsonArc|ElectricBlue|...> (or full AFL.Beam.<color>)."));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetBeam - no game world (run inside PIE)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetBeam - no UAFLCosmeticLoadoutComponent on the local player's PlayerState."));
			return;
		}

		FString IdStr = Args[0].TrimStartAndEnd();
		if (!IdStr.StartsWith(TEXT("AFL.Beam."), ESearchCase::IgnoreCase))
		{
			IdStr = FString::Printf(TEXT("AFL.Beam.%s"), *IdStr);
		}
		const FName BeamId(*IdStr);

		FAFLCosmeticSelection Request = Loadout->GetSelection();
		if (Request.GetActiveIdentityId() == NAME_None)
		{
			Request.IdentityType = EAFLIdentityType::Team;
			Request.TeamId = FName(TEXT("AFL.Team.ARIA"));
		}
		Request.BeamId = BeamId;

		Loadout->ServerSetCosmeticSelection(Request); // PURE: client-issued; server does the rest.

		Ar.Logf(TEXT("afl.Cosmetic.SetBeam - client issued ServerSetCosmeticSelection(beam=%s). Own it first (afl.Wallet.Buy %s). Watch [SkinDiag] RefreshBeamColor/OnRep_BeamColor/ApplyBeamColor with `afl.SkinDiag 1`."),
			*BeamId.ToString(), *BeamId.ToString());
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetBeamCmd(
		TEXT("afl.Cosmetic.SetBeam"),
		TEXT("INDEPENDENT beam axis: client-issued PURE caller of ServerSetCosmeticSelection (sets BeamId -> a beam-color SKU). Independent of the weapon + its skin -> a blue weapon can fire a red beam. Usage: afl.Cosmetic.SetBeam <color> (or full AFL.Beam.<color>). Entitlement-gated: afl.Wallet.Buy AFL.Beam.<color> first."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetBeam));

	// --- WEAPON-SKIN selection seam: afl.Cosmetic.SetWeaponSkin <[Pattern.]Color> (INDEPENDENT WeaponSkinId axis) ---
	// MIRRORS HandleAFLCosmeticSetBeam. Sets WeaponSkinId (AFL.WeaponSkin.<Pattern>.<Color>) -- a weapon skin is its
	// OWN owned item, applies to ANY equipped weapon (OVERRIDES its baked original color), NOT the retired per-weapon
	// AFL.Weapon.<W>.<Color> coupling. A bare "<Color>" arg defaults the NeonCamo pattern.
	void HandleAFLCosmeticSetWeaponSkin(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetWeaponSkin - usage: afl.Cosmetic.SetWeaponSkin <CrimsonArc | NeonCamo.CrimsonArc> (or full AFL.WeaponSkin.<Pattern>.<Color>)."));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetWeaponSkin - no game world (run inside PIE)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetWeaponSkin - no UAFLCosmeticLoadoutComponent on the local player's PlayerState."));
			return;
		}

		FString IdStr = Args[0].TrimStartAndEnd();
		if (!IdStr.StartsWith(TEXT("AFL.WeaponSkin."), ESearchCase::IgnoreCase))
		{
			// bare "<Color>" defaults the NeonCamo pattern; "<Pattern>.<Color>" passes through.
			if (!IdStr.Contains(TEXT(".")))
			{
				IdStr = FString::Printf(TEXT("NeonCamo.%s"), *IdStr);
			}
			IdStr = FString::Printf(TEXT("AFL.WeaponSkin.%s"), *IdStr);
		}
		const FName WeaponSkinId(*IdStr);

		FAFLCosmeticSelection Request = Loadout->GetSelection();
		if (Request.GetActiveIdentityId() == NAME_None)
		{
			Request.IdentityType = EAFLIdentityType::Team;
			Request.TeamId = FName(TEXT("AFL.Team.ARIA"));
		}
		Request.WeaponSkinId = WeaponSkinId;

		Loadout->ServerSetCosmeticSelection(Request); // PURE: client-issued; server does the rest.

		Ar.Logf(TEXT("afl.Cosmetic.SetWeaponSkin - client issued ServerSetCosmeticSelection(weaponSkin=%s). Own it first (afl.Wallet.Buy %s). Watch [SkinDiag] RefreshWeaponSkin/OnRep_WeaponSkin/ApplyWeaponSkin with `afl.SkinDiag 1`."),
			*WeaponSkinId.ToString(), *WeaponSkinId.ToString());
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetWeaponSkinCmd(
		TEXT("afl.Cosmetic.SetWeaponSkin"),
		TEXT("INDEPENDENT weapon-skin axis: client-issued PURE caller of ServerSetCosmeticSelection (sets WeaponSkinId -> the NeonCamo MI on ANY equipped weapon, overriding its baked original). Usage: afl.Cosmetic.SetWeaponSkin <[Pattern.]Color> (e.g. CrimsonArc, NeonCamo.CrimsonArc, or full AFL.WeaponSkin.<Pattern>.<Color>). Entitlement-gated: afl.Wallet.Buy first."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetWeaponSkin));

	// --- WEAPON-EQUIP selection seam: afl.Cosmetic.SetWeapon <Name> (the WeaponId axis) ---
	// MIRRORS HandleAFLCosmeticSetWeaponSkin EXACTLY, but sets WeaponId -- the axis that REPLACES the equipped
	// weapon (RefreshWeaponForPawn -> ResolveAsset -> UAFLWeaponCosmeticAsset -> EquipmentDefinition -> EquipItem)
	// rather than re-tinting whatever is already held.
	//
	// WHY THIS EXISTS (the gap this closes): a UFUNCTION(Exec) sibling already existed --
	// UAFLCombatCheats::SetCosmeticWeapon -- but an Exec on a CheatManagerExtension only routes WHEN THE CHEAT
	// MANAGER IS ACTIVE, and Lyra gates that. Typed in a normal PIE session it produced NO output at all: not the
	// success log, not the no-loadout warning, not even "command not recognized" -- it simply never ran, which
	// reads exactly like "the weapon is broken". Two PIE sessions were spent chasing the data chain before the
	// log showed the Cmd echo with nothing after it. Every OTHER cosmetic axis (Edge/Body/Beam/WeaponSkin/
	// Facemask/Identity/Character) already had an always-on console command for precisely this reason; the
	// weapon-equip axis was the one that never got one. Same rationale as the SetEdge comment above -- and the
	// world-context delegate also resolves the PIE WINDOW's PlayerController, so a command typed in a CLIENT
	// window takes the genuine client->server hop.
	//
	// PURE CALLER, identical contract to its siblings: build FAFLCosmeticSelection from the current replicated
	// selection (don't clobber identity; seed AFL.Team.ARIA if unset so _Validate passes), set WeaponId, hand to
	// ServerSetCosmeticSelection. Server does all validation/entitlement/gating/commit/replicate.
	void HandleAFLCosmeticSetWeapon(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetWeapon - usage: afl.Cosmetic.SetWeapon <BigSixx | Akuma | ZenKoan | Ripsaw | ...> (or full AFL.Weapon.<Name>)."));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetWeapon - no game world (run inside PIE)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetWeapon - no UAFLCosmeticLoadoutComponent on the local player's PlayerState."));
			return;
		}

		// Accept "BigSixx" or the full "AFL.Weapon.BigSixx". CosmeticId comparison is FName-based (case-
		// insensitive), so the caller's casing does not matter.
		FString IdStr = Args[0].TrimStartAndEnd();
		if (!IdStr.StartsWith(TEXT("AFL.Weapon."), ESearchCase::IgnoreCase))
		{
			IdStr = FString::Printf(TEXT("AFL.Weapon.%s"), *IdStr);
		}
		const FName WeaponId(*IdStr);

		FAFLCosmeticSelection Request = Loadout->GetSelection();
		if (Request.GetActiveIdentityId() == NAME_None)
		{
			Request.IdentityType = EAFLIdentityType::Team;
			Request.TeamId = FName(TEXT("AFL.Team.ARIA"));
		}
		Request.WeaponId = WeaponId;

		Loadout->ServerSetCosmeticSelection(Request); // PURE: client-issued; server does the rest.

		Ar.Logf(TEXT("afl.Cosmetic.SetWeapon - client issued ServerSetCosmeticSelection(weapon=%s). Entitlement-gated (GrantedFree weapons need no purchase; otherwise afl.Wallet.Buy %s). Watch [SkinDiag] RefreshWeapon equip the pawn with `afl.SkinDiag 1`."),
			*WeaponId.ToString(), *WeaponId.ToString());
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetWeaponCmd(
		TEXT("afl.Cosmetic.SetWeapon"),
		TEXT("WEAPON-EQUIP axis: client-issued PURE caller of ServerSetCosmeticSelection (sets WeaponId -> REPLACES the equipped weapon via RefreshWeaponForPawn). Usage: afl.Cosmetic.SetWeapon <Name> (e.g. BigSixx, Akuma, or full AFL.Weapon.<Name>). Always available -- unlike the afl.Cosmetic.SetCosmeticWeapon Exec, which only routes when Lyra's cheat manager is active."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetWeapon));

	// --- DUAL-MOUNT LEFT-hand seam: afl.Cosmetic.SetLeftWeapon <Name> (the NEW LeftWeaponId axis) ---
	// MIRRORS HandleAFLCosmeticSetWeapon EXACTLY, but sets LeftWeaponId -- the field that engages the arm-worn
	// Hand-Cannon dual path (RefreshWeaponForPawn dispatches to RefreshHandCannonsForPawn WHENEVER LeftWeaponId is
	// set). The RIGHT hand is still WeaponId: set it FIRST with afl.Cosmetic.SetWeapon (or leave the current
	// selection's WeaponId), then this adds the left cannon so BOTH coexist. Pass "none"/"clear" to drop the left
	// hand back to the single-held path. Test harness for the dual-mount PIE (HANDOFF_AIK_HANDCANNON_DUALMOUNT.md).
	void HandleAFLCosmeticSetLeftWeapon(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetLeftWeapon - usage: afl.Cosmetic.SetLeftWeapon <HandCannon.FANATICS.L | ... | none> (or full AFL.Weapon.<Name>). Set the RIGHT hand first with afl.Cosmetic.SetWeapon."));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetLeftWeapon - no game world (run inside PIE)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetLeftWeapon - no UAFLCosmeticLoadoutComponent on the local player's PlayerState."));
			return;
		}

		// "none"/"clear"/"off" -> NAME_None (drop the left cannon, back to single-held). Otherwise accept
		// "HandCannon.FANATICS.L" or the full "AFL.Weapon.HandCannon.FANATICS.L" (FName compare is case-insensitive).
		FString IdStr = Args[0].TrimStartAndEnd();
		FName LeftWeaponId = NAME_None;
		if (!IdStr.Equals(TEXT("none"), ESearchCase::IgnoreCase)
			&& !IdStr.Equals(TEXT("clear"), ESearchCase::IgnoreCase)
			&& !IdStr.Equals(TEXT("off"), ESearchCase::IgnoreCase))
		{
			if (!IdStr.StartsWith(TEXT("AFL.Weapon."), ESearchCase::IgnoreCase))
			{
				IdStr = FString::Printf(TEXT("AFL.Weapon.%s"), *IdStr);
			}
			LeftWeaponId = FName(*IdStr);
		}

		FAFLCosmeticSelection Request = Loadout->GetSelection();
		if (Request.GetActiveIdentityId() == NAME_None)
		{
			Request.IdentityType = EAFLIdentityType::Team;
			Request.TeamId = FName(TEXT("AFL.Team.ARIA"));
		}
		Request.LeftWeaponId = LeftWeaponId; // WeaponId (right) is left untouched -> set it with SetWeapon.

		Loadout->ServerSetCosmeticSelection(Request); // PURE: client-issued; server does the rest.

		Ar.Logf(TEXT("afl.Cosmetic.SetLeftWeapon - client issued ServerSetCosmeticSelection(left=%s, right=%s). %s Watch [SkinDiag] RefreshHandCannons equip BOTH forearms with `afl.SkinDiag 1`."),
			(LeftWeaponId != NAME_None) ? *LeftWeaponId.ToString() : TEXT("<none>"),
			(Request.WeaponId != NAME_None) ? *Request.WeaponId.ToString() : TEXT("<none -- set with afl.Cosmetic.SetWeapon>"),
			(LeftWeaponId != NAME_None) ? TEXT("DUAL path engaged (both cannons coexist).") : TEXT("Left cleared -> single-held path."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetLeftWeaponCmd(
		TEXT("afl.Cosmetic.SetLeftWeapon"),
		TEXT("DUAL-MOUNT LEFT-hand axis (arm-worn Hand Cannons): client-issued PURE caller of ServerSetCosmeticSelection (sets LeftWeaponId -> engages RefreshHandCannonsForPawn, holding BOTH cannons). Usage: afl.Cosmetic.SetLeftWeapon <HandCannon.FANATICS.L | none> (or full AFL.Weapon.<Name>). Set the RIGHT hand with afl.Cosmetic.SetWeapon first."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetLeftWeapon));

	// =====================================================================================================
	// afl.Cosmetic.Cycle <axis> [holdSeconds] -- the STANDING VOLUME-PROOF HARNESS (reusable per-axis).
	// Auto-cycles EVERY purchasable SKU of one cosmetic AXIS on the equipped weapon: grants ownership (so the
	// entitlement gate passes) + applies each in sequence, holding ~holdSeconds each with an on-screen label,
	// so the operator just WATCHES (no per-SKU typing). DUAL VERIFICATION: (1) the operator watches VISUAL
	// correctness (hue / liquid-glass / legendaries glitch-on); (2) the LOG proves the mechanism -- each apply
	// emits [AFL_TEST_CYCLE] (issued) and, with afl.SkinDiag auto-enabled, the CONSUMER emits [SkinDiag]
	// Refresh...->mic (resolved). AIK reads the log ONLY AFTER PIE closes (never mid-PIE) + correlates the two
	// markers by SKU id -> a SKU flips PIE-PROVEN only when BOTH pass (watched-correct AND resolved-in-log).
	// Parameterized by axis -> reusable for WeaponSkin, Beam, and Pulse (when its axis lands). LISTEN-HOST
	// window (grant = authority). Equip a weapon first (afl.Cosmetic.SetCosmeticWeapon or SetCosmeticWeapon).
	struct FAFLCosmeticCycleState
	{
		TArray<FName> Ids;
		int32 Index = -1;
		FString Axis;
		float Hold = 2.5f;
		FTimerHandle Timer;
		FTimerHandle FireTimer; // beam-axis auto-fire (the beam renders only when the weapon fires)
		TWeakObjectPtr<UAFLCosmeticLoadoutComponent> Loadout;
		TWeakObjectPtr<UWorld> World;
	};
	static TWeakPtr<FAFLCosmeticCycleState> GActiveCosmeticCycle;
	static const uint64 GCosmeticCycleMsgKey = 0x41464C43; // fixed on-screen key -> the label updates in place

	static void CosmeticCycleApplyNext(TSharedPtr<FAFLCosmeticCycleState> State)
	{
		UWorld* W = State.IsValid() ? State->World.Get() : nullptr;
		UAFLCosmeticLoadoutComponent* L = State.IsValid() ? State->Loadout.Get() : nullptr;
		if (!W || !L) { return; } // PIE torn down -> the world's timer manager is gone; nothing to do

		State->Index++;
		if (!State->Ids.IsValidIndex(State->Index))
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(GCosmeticCycleMsgKey, 8.0f, FColor::Green,
					FString::Printf(TEXT("CYCLE [%s] DONE -- %d SKUs. Close PIE; AIK reads [AFL_TEST_CYCLE]+[SkinDiag]."),
						*State->Axis, State->Ids.Num()));
			}
			UE_LOG(LogAFLCombat, Display, TEXT("[AFL_TEST_CYCLE] DONE axis=%s count=%d"), *State->Axis, State->Ids.Num());
			W->GetTimerManager().ClearTimer(State->Timer);
			W->GetTimerManager().ClearTimer(State->FireTimer);
			GActiveCosmeticCycle.Reset();
			return;
		}

		const FName Id = State->Ids[State->Index];
		FAFLCosmeticSelection Req = L->GetSelection();
		if (Req.GetActiveIdentityId() == NAME_None)
		{
			Req.IdentityType = EAFLIdentityType::Team;
			Req.TeamId = FName(TEXT("AFL.Team.ARIA"));
		}
		if (State->Axis.Equals(TEXT("WeaponSkin"), ESearchCase::IgnoreCase)) { Req.WeaponSkinId = Id; }
		else if (State->Axis.Equals(TEXT("Beam"), ESearchCase::IgnoreCase)) { Req.BeamId = Id; }
		L->ServerSetCosmeticSelection(Req);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(GCosmeticCycleMsgKey, State->Hold + 1.0f, FColor::Cyan,
				FString::Printf(TEXT("CYCLE [%s]  %d / %d   %s"), *State->Axis, State->Index + 1, State->Ids.Num(), *Id.ToString()));
		}
		// [AFL_TEST_CYCLE] = issued; the consumer's [SkinDiag] (auto-enabled) logs the resolve. Same SKU id on both.
		UE_LOG(LogAFLCombat, Display, TEXT("[AFL_TEST_CYCLE] axis=%s idx=%d/%d sku=%s issued"),
			*State->Axis, State->Index + 1, State->Ids.Num(), *Id.ToString());
	}

	// Beam-axis AUTO-FIRE: the beam renders only when the weapon FIRES, so a fast repeating timer pulses the
	// equipped weapon's fire GameplayEvent (InputTag.Weapon.Fire -- the same event Lyra bots fire with) during a
	// Beam cycle. The beam sustains + re-reads the cycling LaserTintColor -> the operator just WATCHES (like the
	// skin cycle). Equip a BEAM weapon first. Cooldown-throttled extra fires are harmlessly dropped. WeaponSkin
	// cycles do NOT start this (the mesh is always visible).
	static void CosmeticCycleFire(TSharedPtr<FAFLCosmeticCycleState> State)
	{
		if (!State.IsValid()) { return; }
		UAFLCosmeticLoadoutComponent* L = State->Loadout.Get();
		APlayerState* PS = L ? Cast<APlayerState>(L->GetOwner()) : nullptr;
		APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		if (!Pawn) { return; }
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
		{
			FGameplayEventData Payload;
			ASC->HandleGameplayEvent(FGameplayTag::RequestGameplayTag(FName("InputTag.Weapon.Fire")), &Payload);
		}
	}

	void HandleAFLCosmeticCycle(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.Cycle - usage: afl.Cosmetic.Cycle <WeaponSkin|Beam> [holdSeconds=2.5]. Equip a weapon first; run on the LISTEN-HOST window."));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.Cycle - no game world (run inside PIE)."));
			return;
		}

		const FString Axis = Args[0].TrimStartAndEnd();
		FString Prefix;
		if (Axis.Equals(TEXT("WeaponSkin"), ESearchCase::IgnoreCase)) { Prefix = TEXT("AFL.WeaponSkin."); }
		else if (Axis.Equals(TEXT("Beam"), ESearchCase::IgnoreCase)) { Prefix = TEXT("AFL.Beam."); }
		else { Ar.Logf(TEXT("afl.Cosmetic.Cycle - unknown axis '%s' (WeaponSkin | Beam; Pulse pending its axis)."), *Axis); return; }

		const float Hold = (Args.Num() >= 2) ? FMath::Max(0.5f, FCString::Atof(*Args[1])) : 2.5f;

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
		if (!Loadout || !Wallet)
		{
			Ar.Log(TEXT("afl.Cosmetic.Cycle - no loadout/wallet on the local PlayerState."));
			return;
		}
		if (!PS->HasAuthority())
		{
			Ar.Log(TEXT("afl.Cosmetic.Cycle - run on the LISTEN-HOST window (grant-ownership needs authority)."));
			return;
		}

		UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!Catalog)
		{
			Ar.Log(TEXT("afl.Cosmetic.Cycle - catalog subsystem not ready."));
			return;
		}

		// Enumerate every PURCHASABLE SKU (DIRECT skins/beams) whose id matches the axis prefix, and GRANT each
		// (DebugGrantOwnership -> the entitlement gate passes for the cycle without a real purchase).
		TArray<FAFLCatalogEntry> All;
		Catalog->GetPurchasableEntries(All);
		TSharedPtr<FAFLCosmeticCycleState> State = MakeShared<FAFLCosmeticCycleState>();
		State->Axis = Axis;
		State->Hold = Hold;
		State->Loadout = Loadout;
		State->World = World;
		for (const FAFLCatalogEntry& E : All)
		{
			if (E.CosmeticId.ToString().StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				State->Ids.Add(E.CosmeticId);
				Wallet->DebugGrantOwnership(E.CosmeticId);
			}
		}
		State->Ids.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });

		if (State->Ids.Num() == 0)
		{
			Ar.Logf(TEXT("afl.Cosmetic.Cycle - no SKUs found for axis '%s' (prefix %s). Author/register the SKUs first."), *Axis, *Prefix);
			return;
		}

		// Auto-enable the consumer resolve-diag so the LOG carries [SkinDiag] Refresh...->mic per apply.
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.SkinDiag"))) { CVar->Set(TEXT("1")); }

		GActiveCosmeticCycle = State;
		Ar.Logf(TEXT("afl.Cosmetic.Cycle - START axis=%s count=%d hold=%.1fs granted=%d. WATCH the weapon (each SKU ~%.1fs + on-screen label). Close PIE when done -> AIK reads [AFL_TEST_CYCLE]+[SkinDiag]. afl.Cosmetic.CycleStop to abort."),
			*Axis, State->Ids.Num(), Hold, State->Ids.Num(), Hold);
		UE_LOG(LogAFLCombat, Display, TEXT("[AFL_TEST_CYCLE] START axis=%s count=%d hold=%.1f"), *Axis, State->Ids.Num(), Hold);

		FTimerDelegate Del;
		Del.BindLambda([State]() { CosmeticCycleApplyNext(State); });
		World->GetTimerManager().SetTimer(State->Timer, Del, Hold, /*bLoop*/ true, /*FirstDelay*/ 0.0f);

		// Beam axis: auto-fire fast so the beam is visible without manual firing (equip a beam weapon first).
		if (Axis.Equals(TEXT("Beam"), ESearchCase::IgnoreCase))
		{
			FTimerDelegate FireDel;
			FireDel.BindLambda([State]() { CosmeticCycleFire(State); });
			World->GetTimerManager().SetTimer(State->FireTimer, FireDel, 0.15f, /*bLoop*/ true, /*FirstDelay*/ 0.1f);
		}
	}

	void HandleAFLCosmeticCycleStop(const TArray<FString>& /*Args*/, UWorld* /*World*/, FOutputDevice& Ar)
	{
		TSharedPtr<FAFLCosmeticCycleState> State = GActiveCosmeticCycle.Pin();
		if (State.IsValid() && State->World.IsValid())
		{
			State->World->GetTimerManager().ClearTimer(State->Timer);
			State->World->GetTimerManager().ClearTimer(State->FireTimer);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(GCosmeticCycleMsgKey, 4.0f, FColor::Yellow,
					FString::Printf(TEXT("CYCLE [%s] STOPPED at %d/%d"), *State->Axis, State->Index + 1, State->Ids.Num()));
			}
			Ar.Logf(TEXT("afl.Cosmetic.CycleStop - stopped axis=%s at %d/%d."), *State->Axis, State->Index + 1, State->Ids.Num());
			UE_LOG(LogAFLCombat, Display, TEXT("[AFL_TEST_CYCLE] STOPPED axis=%s at=%d"), *State->Axis, State->Index + 1);
			GActiveCosmeticCycle.Reset();
		}
		else
		{
			Ar.Log(TEXT("afl.Cosmetic.CycleStop - no active cycle."));
		}
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticCycleCmd(
		TEXT("afl.Cosmetic.Cycle"),
		TEXT("STANDING VOLUME-PROOF HARNESS: auto-cycles every SKU of a cosmetic axis (grant+apply each, hold ~2.5s, on-screen label + [AFL_TEST_CYCLE] log; auto-enables afl.SkinDiag for the [SkinDiag] resolve log). Operator WATCHES visual; AIK reads the log AFTER PIE. Reusable per-axis. Usage: afl.Cosmetic.Cycle <WeaponSkin|Beam> [holdSeconds]. Equip a weapon first; LISTEN-HOST window."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticCycle));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticCycleStopCmd(
		TEXT("afl.Cosmetic.CycleStop"),
		TEXT("Abort the active afl.Cosmetic.Cycle harness run."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticCycleStop));

	// ─── THUMBNAIL FRAMING CANARY: afl.Thumbnail.Canary ──────────────────────────
	// STEP 1 of the 261-SKU thumbnail batch (#1/#7): PROVE THE FRAMING before rendering 172. Captures the
	// CURRENT pawn (whatever is equipped) with each render-axis's framing preset onto a near-black IRONICS card
	// and EXPORTS a PNG to <ProjectSaved>/Thumbnails/ -- NO cosmetic apply, NO .uasset/catalog writes (those are
	// the batch's job, deferred until framing is signed off + the editor-asset module deps are wired). The three
	// framing presets are LIVE-TUNABLE via afl.Thumb.* cvars (seeded to the proposed constants) so the operator
	// dials framing in PIE and re-runs -- no rebuild per iteration (the proven afl.Loadout.Preview* pattern).
	// Synchronous: ExportRenderTarget reads-with-flush, so all 5 captures + exports finish inside the command.
	// Engine-only (SceneCapture2D + ExportRenderTarget) -> no new module deps.
	static TAutoConsoleVariable<float> CVarThumbWpnFwd(TEXT("afl.Thumb.Wpn.Fwd"), 150.f, TEXT("Weapon/skin thumb: camera forward offset (cm, pawn-local)."));
	static TAutoConsoleVariable<float> CVarThumbWpnRight(TEXT("afl.Thumb.Wpn.Right"), 60.f, TEXT("Weapon/skin thumb: camera right offset."));
	static TAutoConsoleVariable<float> CVarThumbWpnUp(TEXT("afl.Thumb.Wpn.Up"), 32.f, TEXT("Weapon/skin thumb: camera up offset. (PIE-approved seed)"));
	static TAutoConsoleVariable<float> CVarThumbWpnFOV(TEXT("afl.Thumb.Wpn.FOV"), 32.f, TEXT("Weapon/skin thumb: FOV degrees. (PIE-approved seed)"));
	static TAutoConsoleVariable<float> CVarThumbWpnFocusFwd(TEXT("afl.Thumb.Wpn.FocusFwd"), 30.f, TEXT("Weapon/skin thumb: look-at forward (weapon center)."));
	static TAutoConsoleVariable<float> CVarThumbWpnFocusUp(TEXT("afl.Thumb.Wpn.FocusUp"), 62.f, TEXT("Weapon/skin thumb: look-at up (weapon center). (PIE-approved seed)"));
	static TAutoConsoleVariable<float> CVarThumbPortFwd(TEXT("afl.Thumb.Port.Fwd"), 300.f, TEXT("Identity/finish thumb: camera forward offset."));
	static TAutoConsoleVariable<float> CVarThumbPortRight(TEXT("afl.Thumb.Port.Right"), 120.f, TEXT("Identity/finish thumb: camera right offset."));
	static TAutoConsoleVariable<float> CVarThumbPortUp(TEXT("afl.Thumb.Port.Up"), 55.f, TEXT("Identity/finish thumb: camera up offset."));
	static TAutoConsoleVariable<float> CVarThumbPortFOV(TEXT("afl.Thumb.Port.FOV"), 35.f, TEXT("Identity/finish thumb: FOV degrees."));
	static TAutoConsoleVariable<float> CVarThumbPortFocusUp(TEXT("afl.Thumb.Port.FocusUp"), 40.f, TEXT("Identity/finish thumb: look-at up."));
	static TAutoConsoleVariable<float> CVarThumbFaceFwd(TEXT("afl.Thumb.Face.Fwd"), 80.f, TEXT("Facemask thumb: camera forward offset. (PIE-approved seed)"));
	static TAutoConsoleVariable<float> CVarThumbFaceRight(TEXT("afl.Thumb.Face.Right"), 35.f, TEXT("Facemask thumb: camera right offset."));
	static TAutoConsoleVariable<float> CVarThumbFaceUp(TEXT("afl.Thumb.Face.Up"), 82.f, TEXT("Facemask thumb: camera up offset. (PIE-approved seed)"));
	static TAutoConsoleVariable<float> CVarThumbFaceFOV(TEXT("afl.Thumb.Face.FOV"), 24.f, TEXT("Facemask thumb: FOV degrees."));
	static TAutoConsoleVariable<float> CVarThumbFaceFocusUp(TEXT("afl.Thumb.Face.FocusUp"), 78.f, TEXT("Facemask thumb: look-at up. (PIE-approved seed)"));

	void HandleAFLThumbnailCanary(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Thumbnail.Canary - no game world (run inside PIE)."));
			return;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			Ar.Log(TEXT("afl.Thumbnail.Canary - no local pawn (spawn in first)."));
			return;
		}

		// Isolated scene-capture on the pawn: near-black IRONICS card, robot + equipped weapon only, no pod
		// (thumbnails are clean product shots, not the loadout diorama). Same ShowOnly pattern the loadout
		// preview proves (pawn + attached actors -> the character-part child-actors + the weapon render).
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.Owner = Pawn;
		ASceneCapture2D* Cap = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), SP);
		USceneCaptureComponent2D* CapComp = Cap ? Cap->GetCaptureComponent2D() : nullptr;
		if (!CapComp)
		{
			Ar.Log(TEXT("afl.Thumbnail.Canary - failed to spawn scene capture."));
			if (Cap) { Cap->Destroy(); }
			return;
		}
		Cap->AttachToActor(Pawn, FAttachmentTransformRules::KeepRelativeTransform);

		TArray<AActor*> ShowOnly;
		ShowOnly.Add(Pawn);
		{
			TArray<AActor*> Attached;
			Pawn->GetAttachedActors(Attached);
			ShowOnly.Append(Attached);
		}

		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Cap);
		// 8-bit LDR so UKismetRenderingLibrary::ExportRenderTarget emits a REAL PNG. The default RTF_RGBA16f makes
		// it take the HDR branch and write EXR bytes into the .png -> viewers report "corrupt / unsupported".
		RT->RenderTargetFormat = RTF_RGBA8;
		RT->ClearColor = FLinearColor(0.02f, 0.02f, 0.03f, 1.f); // near-black IRONICS card
		CapComp->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR; // lit + tonemapped (neon reads true)
		CapComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		CapComp->ShowOnlyActors = ShowOnly;
		CapComp->bCaptureEveryFrame = false;
		CapComp->bCaptureOnMovement = false;
		CapComp->ShowFlags.SetAtmosphere(false);
		CapComp->ShowFlags.SetFog(false);
		CapComp->ShowFlags.SetVolumetricFog(false);
		CapComp->ShowFlags.SetCloud(false);

		struct FThumbPreset { const TCHAR* Label; FVector Cam; FVector Focus; float FOV; int32 W; int32 H; };
		const FVector WpnCam(CVarThumbWpnFwd.GetValueOnGameThread(), CVarThumbWpnRight.GetValueOnGameThread(), CVarThumbWpnUp.GetValueOnGameThread());
		const FVector WpnFocus(CVarThumbWpnFocusFwd.GetValueOnGameThread(), 0.f, CVarThumbWpnFocusUp.GetValueOnGameThread());
		const FVector PortCam(CVarThumbPortFwd.GetValueOnGameThread(), CVarThumbPortRight.GetValueOnGameThread(), CVarThumbPortUp.GetValueOnGameThread());
		const FVector PortFocus(0.f, 0.f, CVarThumbPortFocusUp.GetValueOnGameThread());
		const FVector FaceCam(CVarThumbFaceFwd.GetValueOnGameThread(), CVarThumbFaceRight.GetValueOnGameThread(), CVarThumbFaceUp.GetValueOnGameThread());
		const FVector FaceFocus(0.f, 0.f, CVarThumbFaceFocusUp.GetValueOnGameThread());
		const FThumbPreset Presets[] = {
			{ TEXT("Weapon"),     WpnCam,  WpnFocus,  CVarThumbWpnFOV.GetValueOnGameThread(),  768, 512 },
			{ TEXT("WeaponSkin"), WpnCam,  WpnFocus,  CVarThumbWpnFOV.GetValueOnGameThread(),  768, 512 },
			{ TEXT("Identity"),   PortCam, PortFocus, CVarThumbPortFOV.GetValueOnGameThread(), 512, 768 },
			{ TEXT("Finish"),     PortCam, PortFocus, CVarThumbPortFOV.GetValueOnGameThread(), 512, 768 },
			{ TEXT("Facemask"),   FaceCam, FaceFocus, CVarThumbFaceFOV.GetValueOnGameThread(), 512, 768 },
		};

		const FString OutDir = FPaths::ProjectSavedDir() / TEXT("Thumbnails");
		IFileManager::Get().MakeDirectory(*OutDir, /*Tree*/ true);
		int32 Count = 0;
		for (const FThumbPreset& P : Presets)
		{
			RT->InitCustomFormat(P.W, P.H, PF_B8G8R8A8, /*bInForceLinearGamma*/ false);
			RT->UpdateResourceImmediate(true);
			CapComp->TextureTarget = RT;
			CapComp->FOVAngle = P.FOV;
			Cap->SetActorRelativeLocation(P.Cam);
			Cap->SetActorRelativeRotation((P.Focus - P.Cam).Rotation());
			CapComp->CaptureScene(); // synchronous render into RT

			const FString FileName = FString::Printf(TEXT("CANARY_%s.png"), P.Label);
			UKismetRenderingLibrary::ExportRenderTarget(World, RT, OutDir, FileName); // reads-with-flush -> PNG
			Ar.Logf(TEXT("afl.Thumbnail.Canary - %s (%dx%d, FOV %.0f) -> %s/%s"), P.Label, P.W, P.H, P.FOV, *OutDir, *FileName);
			++Count;
		}

		Cap->Destroy();
		Ar.Logf(TEXT("afl.Thumbnail.Canary - DONE: %d framings -> %s . Open the PNGs, tune afl.Thumb.* cvars, re-run (no rebuild)."), Count, *OutDir);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
				FString::Printf(TEXT("THUMB CANARY: %d PNGs -> Saved/Thumbnails/ (tune afl.Thumb.* + re-run)"), Count));
		}
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLThumbnailCanaryCmd(
		TEXT("afl.Thumbnail.Canary"),
		TEXT("Thumbnail FRAMING proof (#1/#7): capture the CURRENT pawn with each axis framing preset -> PNG in Saved/Thumbnails/. No apply/asset writes. Tune afl.Thumb.Wpn/Port/Face.* cvars + re-run. Usage: afl.Thumbnail.Canary (run in PIE with a weapon equipped)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLThumbnailCanary));

	// ─── THUMBNAIL WRITE-CHAIN CANARY: afl.Thumbnail.Batch <axis> ─────────────────
	// STEP 3 (#1/#7): prove the WRITE CHAIN on ONE axis (Weapon = 13) before the 172. Per SKU:
	//   grant (DebugGrantOwnership) -> equip via the PROVEN ServerSetCosmeticSelection path -> SETTLE one Hold
	//   tick (equip is async: server -> OnRep -> Refresh spawns the new weapon actor) -> CaptureScene the
	//   settled pawn+weapon -> ConstructTexture2D -> SavePackage /Game/AFL/UI/Thumbnails/T_Thumb_<SKU>.
	// Timer-driven (Cycle pattern): capture LAGS equip by one tick so every shot is a fully-settled weapon. The
	// ShopThumbnail -> DA_AFL_CosmeticCatalog assignment is a SEPARATE bridge step (editor-idle) -- safer than
	// saving the shared LFS 261-entry DA from inside a running PIE session, and disk-verifiable.
	// GRANT TELL (banked lesson): the 13 thumbnails must each show a DIFFERENT weapon; all-identical = grant not firing.
	struct FAFLThumbBatchState
	{
		TArray<FName> Ids;
		int32 EquipIdx = -1; // the SKU equipped last tick -> capture it THIS tick (settled)
		int32 Saved = 0;
		FString Axis;
		FTimerHandle Timer;
		float Hold = 1.0f;
		FVector Cam = FVector::ZeroVector;
		FVector Focus = FVector::ZeroVector;
		float FOV = 28.f;
		int32 W = 768;
		int32 H = 512;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<APawn> Pawn;
		TWeakObjectPtr<UAFLCosmeticLoadoutComponent> Loadout;
		TWeakObjectPtr<ASceneCapture2D> Cap;
		TObjectPtr<UTextureRenderTarget2D> RT; // kept alive by CapComp->TextureTarget (a UPROPERTY)
	};
	static TSharedPtr<FAFLThumbBatchState> GActiveThumbBatch;

	static FString ThumbLastToken(const FName& Id) // short label for on-screen display only
	{
		const FString Str = Id.ToString();
		FString Left, Right;
		return Str.Split(TEXT("."), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd) ? Right : Str;
	}

	// UNIQUE per-SKU asset name from the FULL CosmeticId -- the last token alone COLLIDES (WeaponSkin =
	// AFL.WeaponSkin.<Pattern>.<Color> and Finish = AFL.Finish.<Color> share color names). "AFL.WeaponSkin.
	// NeonCamo.Amber" -> "WeaponSkin_NeonCamo_Amber". The DA-assign bridge applies the SAME transform so refs match.
	static FString ThumbAssetName(const FName& Id)
	{
		FString Str = Id.ToString();
		Str.RemoveFromStart(TEXT("AFL."), ESearchCase::IgnoreCase);
		Str.ReplaceInline(TEXT("."), TEXT("_"));
		return Str;
	}

	static void ThumbBatchCaptureAndSave(TSharedPtr<FAFLThumbBatchState> S, const FName& Id)
	{
		APawn* Pawn = S->Pawn.Get();
		ASceneCapture2D* Cap = S->Cap.Get();
		USceneCaptureComponent2D* CapComp = Cap ? Cap->GetCaptureComponent2D() : nullptr;
		if (!Pawn || !CapComp || !S->RT) { return; }

		// Re-frame (axis preset) + REFRESH the isolate list -- the equipped weapon actor changed for this SKU.
		// The RT is sized + initialized ONCE in START (a registered capture with an uninitialized target renders
		// into a null RenderTarget on the next Draw -> the crash), so there is no per-capture re-init here.
		CapComp->FOVAngle = S->FOV;
		Cap->SetActorRelativeLocation(S->Cam);
		Cap->SetActorRelativeRotation((S->Focus - S->Cam).Rotation());
		TArray<AActor*> ShowOnly;
		ShowOnly.Add(Pawn);
		{ TArray<AActor*> Att; Pawn->GetAttachedActors(Att); ShowOnly.Append(Att); }
		CapComp->ShowOnlyActors = ShowOnly;
		CapComp->CaptureScene();

#if WITH_EDITOR
		const FString SkuName = ThumbAssetName(Id); // full-id -> unique (last-token collides across WeaponSkin/Finish)
		const FString PkgName = FString::Printf(TEXT("/Game/AFL/UI/Thumbnails/T_Thumb_%s"), *SkuName);
		UPackage* Pkg = CreatePackage(*PkgName);
		if (!Pkg) { UE_LOG(LogAFLCombat, Warning, TEXT("[ThumbBatch] CreatePackage FAILED %s"), *PkgName); return; }
		Pkg->FullyLoad();
		UTexture2D* Tex = S->RT->ConstructTexture2D(Pkg, FString::Printf(TEXT("T_Thumb_%s"), *SkuName),
			RF_Public | RF_Standalone, CTF_Default | CTF_SRGB);
		if (!Tex) { UE_LOG(LogAFLCombat, Warning, TEXT("[ThumbBatch] ConstructTexture2D FAILED %s"), *SkuName); return; }
		Tex->LODGroup = TEXTUREGROUP_UI;
		Tex->MipGenSettings = TMGS_NoMipmaps;
		Tex->PostEditChange();
		Pkg->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const FString PkgFile = FPackageName::LongPackageNameToFilename(PkgName, FPackageName::GetAssetPackageExtension());
		const bool bSaved = UPackage::SavePackage(Pkg, Tex, *PkgFile, SaveArgs);
		if (bSaved) { FAssetRegistryModule::AssetCreated(Tex); }
		UE_LOG(LogAFLCombat, Display, TEXT("[ThumbBatch] sku=%s -> %s (%dx%d) saved=%s"),
			*Id.ToString(), *PkgName, S->W, S->H, bSaved ? TEXT("YES") : TEXT("NO"));
#else
		UE_LOG(LogAFLCombat, Warning, TEXT("[ThumbBatch] texture save is editor-only (WITH_EDITOR)."));
#endif
	}

	static void ThumbBatchEquip(TSharedPtr<FAFLThumbBatchState> S, const FName& Id)
	{
		UAFLCosmeticLoadoutComponent* L = S->Loadout.Get();
		if (!L) { return; }
		FAFLCosmeticSelection Req = L->GetSelection();
		if (Req.GetActiveIdentityId() == NAME_None) { Req.IdentityType = EAFLIdentityType::Team; Req.TeamId = FName(TEXT("AFL.Team.ARIA")); }
		const FString& Axis = S->Axis;
		if      (Axis.Equals(TEXT("Weapon"), ESearchCase::IgnoreCase))     { Req.WeaponId = Id; }
		else if (Axis.Equals(TEXT("WeaponSkin"), ESearchCase::IgnoreCase)) { Req.WeaponSkinId = Id; }
		else if (Axis.Equals(TEXT("Finish"), ESearchCase::IgnoreCase))     { Req.BodyId = Id; }
		else if (Axis.Equals(TEXT("Facemask"), ESearchCase::IgnoreCase))   { Req.FacemaskId = Id; }
		else if (Axis.Equals(TEXT("Identity"), ESearchCase::IgnoreCase))
		{
			// Dual-type: a Character SKU sets IdentityType=Character+CharacterId; a Team SKU sets Team+TeamId.
			if (Id.ToString().StartsWith(TEXT("AFL.Character."), ESearchCase::IgnoreCase)) { Req.IdentityType = EAFLIdentityType::Character; Req.CharacterId = Id; }
			else { Req.IdentityType = EAFLIdentityType::Team; Req.TeamId = Id; }
		}
		L->ServerSetCosmeticSelection(Req);
		UE_LOG(LogAFLCombat, Display, TEXT("[ThumbBatch] equip sku=%s (settle %.1fs)"), *Id.ToString(), S->Hold);
	}

	static void ThumbBatchStep(TSharedPtr<FAFLThumbBatchState> S)
	{
		UWorld* W = S.IsValid() ? S->World.Get() : nullptr;
		if (!W || !S->Loadout.IsValid() || !S->Cap.IsValid()) { GActiveThumbBatch.Reset(); return; }

		// 1. Capture the PREVIOUSLY-equipped (now Hold-settled) SKU.
		if (S->Ids.IsValidIndex(S->EquipIdx))
		{
			ThumbBatchCaptureAndSave(S, S->Ids[S->EquipIdx]);
			S->Saved++;
		}
		// 2. Advance + equip the next, or finish.
		S->EquipIdx++;
		if (!S->Ids.IsValidIndex(S->EquipIdx))
		{
			if (ASceneCapture2D* Cap = S->Cap.Get()) { Cap->Destroy(); }
			W->GetTimerManager().ClearTimer(S->Timer);
			UE_LOG(LogAFLCombat, Display, TEXT("[ThumbBatch] DONE axis=%s saved=%d/%d -> /Game/AFL/UI/Thumbnails/ . Close PIE; check the T_Thumb_* assets, then run the bridge DA-assign."), *S->Axis, S->Saved, S->Ids.Num());
			if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Green, FString::Printf(TEXT("THUMB BATCH [%s] DONE: %d/%d saved"), *S->Axis, S->Saved, S->Ids.Num())); }
			GActiveThumbBatch.Reset();
			return;
		}
		ThumbBatchEquip(S, S->Ids[S->EquipIdx]);
		if (GEngine) { GEngine->AddOnScreenDebugMessage((uint64)0x54484200, S->Hold + 1.f, FColor::Cyan, FString::Printf(TEXT("THUMB BATCH [%s] %d/%d  %s"), *S->Axis, S->EquipIdx + 1, S->Ids.Num(), *ThumbLastToken(S->Ids[S->EquipIdx]))); }
	}

	// Axis -> capture framing (reads the tuned afl.Thumb.* cvars). Weapon/WeaponSkin = landscape weapon preset;
	// Identity/Finish = portrait full-robot; Facemask = portrait head. Size is per-axis (the RT is init'd to it).
	static void ThumbAxisFraming(const FString& Axis, FVector& Cam, FVector& Focus, float& FOV, int32& W, int32& H)
	{
		if (Axis.Equals(TEXT("Facemask"), ESearchCase::IgnoreCase))
		{
			Cam = FVector(CVarThumbFaceFwd.GetValueOnGameThread(), CVarThumbFaceRight.GetValueOnGameThread(), CVarThumbFaceUp.GetValueOnGameThread());
			Focus = FVector(0.f, 0.f, CVarThumbFaceFocusUp.GetValueOnGameThread());
			FOV = CVarThumbFaceFOV.GetValueOnGameThread(); W = 512; H = 768;
		}
		else if (Axis.Equals(TEXT("Identity"), ESearchCase::IgnoreCase) || Axis.Equals(TEXT("Finish"), ESearchCase::IgnoreCase))
		{
			Cam = FVector(CVarThumbPortFwd.GetValueOnGameThread(), CVarThumbPortRight.GetValueOnGameThread(), CVarThumbPortUp.GetValueOnGameThread());
			Focus = FVector(0.f, 0.f, CVarThumbPortFocusUp.GetValueOnGameThread());
			FOV = CVarThumbPortFOV.GetValueOnGameThread(); W = 512; H = 768;
		}
		else // Weapon / WeaponSkin
		{
			Cam = FVector(CVarThumbWpnFwd.GetValueOnGameThread(), CVarThumbWpnRight.GetValueOnGameThread(), CVarThumbWpnUp.GetValueOnGameThread());
			Focus = FVector(CVarThumbWpnFocusFwd.GetValueOnGameThread(), 0.f, CVarThumbWpnFocusUp.GetValueOnGameThread());
			FOV = CVarThumbWpnFOV.GetValueOnGameThread(); W = 768; H = 512;
		}
	}

	void HandleAFLThumbnailBatch(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1) { Ar.Log(TEXT("afl.Thumbnail.Batch - usage: afl.Thumbnail.Batch <Weapon|WeaponSkin|Identity|Finish|Facemask>. LISTEN-HOST window, pawn spawned.")); return; }
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Thumbnail.Batch - run inside PIE.")); return; }
		const FString Axis = Args[0].TrimStartAndEnd();
		// (Type, id-prefix) sources to enumerate + grant. Most axes = one; Identity = dual-type (Team + Character).
		TArray<TPair<EAFLCosmeticType, FString>> Sources;
		if      (Axis.Equals(TEXT("Weapon"), ESearchCase::IgnoreCase))     { Sources.Emplace(EAFLCosmeticType::Weapon,   TEXT("AFL.Weapon.")); }
		else if (Axis.Equals(TEXT("WeaponSkin"), ESearchCase::IgnoreCase)) { Sources.Emplace(EAFLCosmeticType::Weapon,   TEXT("AFL.WeaponSkin.")); }
		else if (Axis.Equals(TEXT("Finish"), ESearchCase::IgnoreCase))     { Sources.Emplace(EAFLCosmeticType::Finish,   TEXT("AFL.Finish.")); }
		else if (Axis.Equals(TEXT("Facemask"), ESearchCase::IgnoreCase))   { Sources.Emplace(EAFLCosmeticType::Facemask, TEXT("AFL.Facemask.")); }
		else if (Axis.Equals(TEXT("Identity"), ESearchCase::IgnoreCase))   { Sources.Emplace(EAFLCosmeticType::Team, TEXT("AFL.Team.")); Sources.Emplace(EAFLCosmeticType::Character, TEXT("AFL.Character.")); }
		else { Ar.Logf(TEXT("afl.Thumbnail.Batch - axis '%s' not wired (Weapon|WeaponSkin|Identity|Finish|Facemask)."), *Axis); return; }

		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
		UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!Pawn || !Loadout || !Wallet || !Catalog) { Ar.Log(TEXT("afl.Thumbnail.Batch - need pawn + loadout + wallet + catalog (run in PIE).")); return; }
		if (!PS->HasAuthority()) { Ar.Log(TEXT("afl.Thumbnail.Batch - run on the LISTEN-HOST window (grant needs authority).")); return; }
		if (GActiveThumbBatch.IsValid() && !GActiveThumbBatch->World.IsValid()) { GActiveThumbBatch.Reset(); } // stale after a PIE abort
		if (GActiveThumbBatch.IsValid()) { Ar.Log(TEXT("afl.Thumbnail.Batch - a batch is already running (afl.Cosmetic.CycleStop won't stop it; close PIE to reset).")); return; }

		// Enumerate + GRANT the axis SKUs (unowned ids no-op the equip -> would capture the default; the banked tell).
		TSharedPtr<FAFLThumbBatchState> S = MakeShared<FAFLThumbBatchState>();
		for (const TPair<EAFLCosmeticType, FString>& Src : Sources)
		{
			TArray<const FAFLCatalogEntry*> Entries;
			Catalog->GetEntriesByType(Src.Key, Entries);
			for (const FAFLCatalogEntry* E : Entries)
			{
				if (!E) { continue; }
				if (E->CosmeticId.ToString().StartsWith(Src.Value, ESearchCase::IgnoreCase))
				{
					S->Ids.Add(E->CosmeticId);
					Wallet->DebugGrantOwnership(E->CosmeticId);
				}
			}
		}
		S->Ids.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
		if (S->Ids.Num() == 0) { Ar.Logf(TEXT("afl.Thumbnail.Batch - no SKUs for axis '%s'."), *Axis); return; }

		// Isolated capture (weapon framing preset from the canary cvars).
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.Owner = Pawn;
		ASceneCapture2D* Cap = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), SP);
		USceneCaptureComponent2D* CapComp = Cap ? Cap->GetCaptureComponent2D() : nullptr;
		if (!CapComp) { Ar.Log(TEXT("afl.Thumbnail.Batch - failed to spawn capture.")); if (Cap) { Cap->Destroy(); } return; }
		Cap->AttachToActor(Pawn, FAttachmentTransformRules::KeepRelativeTransform);

		// Axis framing preset (camera + SIZE) -- computed BEFORE the RT so it is initialized to the right size.
		FVector PCam, PFocus; float PFOV; int32 PW, PH;
		ThumbAxisFraming(Axis, PCam, PFocus, PFOV, PW, PH);

		// The RT MUST be sized/initialized BEFORE it is attached as the capture target: a registered SceneCapture
		// with an UNINITIALIZED TextureTarget renders into a NULL RenderTarget on the next Draw -> access violation
		// (the first attempt's crash). One fixed size per per-axis batch run (weapon 768x512, portrait/face 512x768).
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Cap);
		RT->RenderTargetFormat = RTF_RGBA8;
		RT->ClearColor = FLinearColor(0.02f, 0.02f, 0.03f, 1.f);
		RT->InitCustomFormat(PW, PH, PF_B8G8R8A8, /*bInForceLinearGamma*/ false);
		RT->UpdateResourceImmediate(true);

		CapComp->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		CapComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		CapComp->bCaptureEveryFrame = false;
		CapComp->bCaptureOnMovement = false;
		CapComp->ShowFlags.SetAtmosphere(false);
		CapComp->ShowFlags.SetFog(false);
		CapComp->ShowFlags.SetVolumetricFog(false);
		CapComp->ShowFlags.SetCloud(false);
		{ TArray<AActor*> InitShow; InitShow.Add(Pawn); TArray<AActor*> Att; Pawn->GetAttachedActors(Att); InitShow.Append(Att); CapComp->ShowOnlyActors = InitShow; }
		CapComp->TextureTarget = RT; // now valid + sized -> the registered capture is safe; also GC-keeps the RT

		S->Axis = Axis;
		S->World = World; S->Pawn = Pawn; S->Loadout = Loadout; S->Cap = Cap; S->RT = RT;
		S->Cam = PCam; S->Focus = PFocus; S->FOV = PFOV; S->W = PW; S->H = PH;
		GActiveThumbBatch = S;

		Ar.Logf(TEXT("afl.Thumbnail.Batch - START axis=%s count=%d granted. WATCH: the %d captures must each be DIFFERENT (the grant check). Close PIE when DONE; check /Game/AFL/UI/Thumbnails/T_Thumb_*."), *Axis, S->Ids.Num(), S->Ids.Num());
		UE_LOG(LogAFLCombat, Display, TEXT("[ThumbBatch] START axis=%s count=%d"), *Axis, S->Ids.Num());

		FTimerDelegate Del;
		TWeakPtr<FAFLThumbBatchState> WS = S;
		Del.BindLambda([WS]() { TSharedPtr<FAFLThumbBatchState> P = WS.Pin(); if (P.IsValid()) { ThumbBatchStep(P); } });
		World->GetTimerManager().SetTimer(S->Timer, Del, S->Hold, /*bLoop*/ true, /*FirstDelay*/ 0.25f);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLThumbnailBatchCmd(
		TEXT("afl.Thumbnail.Batch"),
		TEXT("Thumbnail WRITE-CHAIN canary (#1/#7): grant+equip+settle+capture+ConstructTexture2D+SavePackage each SKU of an axis -> /Game/AFL/UI/Thumbnails/T_Thumb_<SKU>. Weapon only (13) for the canary; ShopThumbnail->DA assign is a separate bridge step. Usage: afl.Thumbnail.Batch Weapon (LISTEN-HOST window)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLThumbnailBatch));

	// ─── FACEMASK selection seam: afl.Cosmetic.SetFacemask <Name|none> ───────────
	// MIRRORS HandleAFLCosmeticSetEdge EXACTLY (the proven read-full -> set-one-field -> push pattern). The
	// only deltas: sets Request.FacemaskId (the new axis), normalizes to AFL.Facemask.<Name>, and accepts
	// "none"/"off"/"clear" -> NAME_None to UN-EQUIP. The whole runtime equip path then runs server-side:
	// ServerSetCosmeticSelection (entitlement-gated) -> OnRep -> RefreshFacemaskForPawn -> SetFacemask ->
	// slot-1 material swap + finish re-layer. Dev-equip now; the eventual wallet UI calls the SAME RPC.
	void HandleAFLCosmeticSetFacemask(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetFacemask - usage: <full row id | unique bare name | none | verify>. e.g. AFL.Facemask.Flag.Japan, AFL.Facemask.IroVisor, or bare Japan. Ids resolve against the CATALOG; 31 of 60 rows are namespaced, so a bare guess may not exist."));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetFacemask — no game world (run inside PIE)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetFacemask — no UAFLCosmeticLoadoutComponent on the local player's PlayerState."));
			return;
		}

		// CATALOG-BACKED RESOLUTION (CC-X15). The old code RECONSTRUCTED an id by string --
		// "AFL.Facemask." + <Name> -- which assumes every row is FLAT. Measured: 31 of the 60 catalog rows are
		// NAMESPACED (AFL.Facemask.Basic.Visor, .Tech.Circuit, .Flag.Japan), so the command silently failed on
		// half the catalog, and the ids its own help text advertised (JapanSolar, Kawaii) do not resolve at all.
		// A non-resolving id equipped NOTHING and the run then reported whatever the default state was --
		// indistinguishable from a pass. Resolve against the catalog instead of inventing the id.
		UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!Catalog)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetFacemask - no catalog subsystem; NOTHING EQUIPPED."));
			return;
		}
		// GATHER BY ID PREFIX, NOT BY TYPE (CC-X15b). GetEntriesByType(Facemask) returned only 33 of the 60
		// AFL.Facemask.* rows: 27 of them are typed SkinColor_Edge, left behind by the migration to the dedicated
		// Facemask type (see the enum comment in AFLCosmeticCoreTypes.h). Filtering by type therefore hides half
		// the catalog from this command. Matching AFLCosmeticBrowserLibrary:99, address rows by their id
		// namespace instead -- an id prefix is what actually defines the axis here.
		// Union over EVERY type rather than the two seen, so a row mistyped to a THIRD type is still reached;
		// there is no get-all accessor on the subsystem.
		TArray<const FAFLCatalogEntry*> FaceRows;
		{
			TSet<FName> Seen;
			const UEnum* TypeEnum = StaticEnum<EAFLCosmeticType>();
			const int32 NumTypes = TypeEnum ? TypeEnum->NumEnums() : 0;
			for (int32 T = 0; T < NumTypes; ++T)
			{
				TArray<const FAFLCatalogEntry*> OfType;
				Catalog->GetEntriesByType(static_cast<EAFLCosmeticType>(TypeEnum->GetValueByIndex(T)), OfType);
				for (const FAFLCatalogEntry* R : OfType)
				{
					if (!R) { continue; }
					if (!R->CosmeticId.ToString().StartsWith(TEXT("AFL.Facemask."), ESearchCase::IgnoreCase)) { continue; }
					bool bDup = false;
					Seen.Add(R->CosmeticId, &bDup);
					if (!bDup) { FaceRows.Add(R); }
				}
			}
		}
		// TYPE DISTRIBUTION on every invocation. The prefix gather makes the command work DESPITE the data
		// defect; emitting the split keeps the defect visible instead of papering over it. Expect this to read
		// FACEMASK=33 SKIN_COLOR_EDGE=27 until the 27 rows are retyped.
		{
			TMap<FString, int32> ByType;
			const UEnum* TypeEnum = StaticEnum<EAFLCosmeticType>();
			for (const FAFLCatalogEntry* R : FaceRows)
			{
				const FString TName = TypeEnum ? TypeEnum->GetNameStringByValue((int64)R->Type) : TEXT("?");
				ByType.FindOrAdd(TName)++;
			}
			FString Split;
			for (const TPair<FString, int32>& KV : ByType) { Split += FString::Printf(TEXT("%s=%d "), *KV.Key, KV.Value); }
			Ar.Logf(TEXT("AFL_TEST[FACEMASKTYPES] rows=%d %s"), FaceRows.Num(), *Split);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[FACEMASKTYPES] rows=%d %s"), FaceRows.Num(), *Split);
		}

		// Last dot-segment of a row id, e.g. AFL.Facemask.Flag.Japan -> Japan.
		auto BareOf = [](const FName& Id)
		{
			const FString Full = Id.ToString();
			int32 Dot = INDEX_NONE;
			return Full.FindLastChar(TCHAR(0x2E), Dot) ? Full.RightChop(Dot + 1) : Full;
		};

		FString IdStr = Args[0].TrimStartAndEnd();

		// VERIFY MODE (step 4): push EVERY catalog row id, and every row bare suffix, through the same
		// resolution rules used below. Proves the command can reach the whole catalog rather than asserting it.
		if (IdStr.Equals(TEXT("verify"), ESearchCase::IgnoreCase))
		{
			int32 FullOk = 0, BareUnique = 0, BareAmbig = 0;
			for (const FAFLCatalogEntry* R : FaceRows)
			{
				if (!R) { continue; }
				if (Catalog->FindEntry(R->CosmeticId)) { ++FullOk; }
				const FString Bare = BareOf(R->CosmeticId);
				int32 Hits = 0;
				for (const FAFLCatalogEntry* S : FaceRows)
				{
					if (S && BareOf(S->CosmeticId).Equals(Bare, ESearchCase::IgnoreCase)) { ++Hits; }
				}
				if (Hits == 1) { ++BareUnique; } else if (Hits > 1) { ++BareAmbig; }
				Ar.Logf(TEXT("  row=%s bare=%s bareHits=%d"), *R->CosmeticId.ToString(), *Bare, Hits);
			}
			Ar.Logf(TEXT("AFL_TEST[FACEMASKVERIFY] rows=%d fullIdResolves=%d bareUnique=%d bareAmbiguous=%d"),
				FaceRows.Num(), FullOk, BareUnique, BareAmbig);
			return;
		}

		FName FacemaskId = NAME_None; // un-equip default for none/off/clear
		if (!IdStr.Equals(TEXT("none"), ESearchCase::IgnoreCase)
			&& !IdStr.Equals(TEXT("off"), ESearchCase::IgnoreCase)
			&& !IdStr.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
		{
			// 1. EXACT row id.
			for (const FAFLCatalogEntry* R : FaceRows)
			{
				if (R && R->CosmeticId.ToString().Equals(IdStr, ESearchCase::IgnoreCase)) { FacemaskId = R->CosmeticId; break; }
			}
			// 2. BARE NAME matching exactly one row last segment. AMBIGUOUS -> ERROR, never a guess: silently
			//    picking one would recreate "equipped something, reported a pass" in a new place.
			if (FacemaskId == NAME_None)
			{
				TArray<FName> Matches;
				for (const FAFLCatalogEntry* R : FaceRows)
				{
					if (R && BareOf(R->CosmeticId).Equals(IdStr, ESearchCase::IgnoreCase)) { Matches.Add(R->CosmeticId); }
				}
				if (Matches.Num() == 1)
				{
					FacemaskId = Matches[0];
				}
				else if (Matches.Num() > 1)
				{
					Ar.Logf(TEXT("afl.Cosmetic.SetFacemask - AMBIGUOUS bare name %s matches %d rows; NOTHING EQUIPPED. Use the full row id:"), *IdStr, Matches.Num());
					for (const FName& M : Matches) { Ar.Logf(TEXT("    %s"), *M.ToString()); }
					return;
				}
			}
			// 3. LOUD FAILURE. Never fall through to a request the server drops in silence -- that is the exact
			//    shape that makes a run report the default state and read as a pass.
			if (FacemaskId == NAME_None)
			{
				Ar.Logf(TEXT("afl.Cosmetic.SetFacemask - %s DOES NOT RESOLVE to any of the %d catalog Facemask rows; NOTHING EQUIPPED."), *IdStr, FaceRows.Num());
				Ar.Log(TEXT("  valid ids (first 8; run `afl.Cosmetic.SetFacemask verify` for all):"));
				for (int32 i = 0; i < FMath::Min(8, FaceRows.Num()); ++i)
				{
					if (FaceRows[i]) { Ar.Logf(TEXT("    %s"), *FaceRows[i]->CosmeticId.ToString()); }
				}
				return;
			}
		}

		FAFLCosmeticSelection Request = Loadout->GetSelection();
		if (Request.GetActiveIdentityId() == NAME_None)
		{
			Request.IdentityType = EAFLIdentityType::Team;
			Request.TeamId = FName(TEXT("AFL.Team.ARIA"));
		}
		Request.FacemaskId = FacemaskId;

		Loadout->ServerSetCosmeticSelection(Request); // PURE: client-issued; server does the rest.

		Ar.Logf(TEXT("afl.Cosmetic.SetFacemask — client issued ServerSetCosmeticSelection(facemask=%s). Watch [SkinDiag] RefreshFacemask/OnRep_Facemask/ApplyFacemask with `afl.SkinDiag 1`."),
			(FacemaskId != NAME_None) ? *FacemaskId.ToString() : TEXT("<none/un-equip>"));
	}

	// CC-X17 LINT: catch the DETECTABLE subset of the Type-default trap -- rows whose Type
	// disagrees with their id namespace. FAFLCatalogEntry::Type defaulted to SkinColor_Edge (a real,
	// wrong value) until 2026-08-19, and silently absorbed two separate batches of 27: the
	// AFL.Facemask.* rows and the AFL.Character.*_X rows. The default is now Invalid, so NEW untyped
	// rows are categorically detectable -- but rows authored under the old default are not, and this
	// lint is what finds them.
	//
	// LIMIT, stated so nobody over-trusts it: this compares Type against the ID PREFIX. It cannot
	// catch a row that is genuinely and wrongly SkinColor_Edge with an AFL.Edge.* id -- that is a
	// provenance question and no value read answers it. It catches disagreement, not wrongness.
	//
	// KNOWN POSITIVES it must reproduce or it is broken before it is used:
	//   27x AFL.Character.*_X  typed SkinColor_Edge  (CC-X18, deliberately NOT retyped)
	//    0x AFL.Facemask.*                            (retyped at cc-x16-done)
	void HandleAFLCatalogTypeLint(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UAFLCosmeticCatalogSubsystem* Catalog = World ? UAFLCosmeticCatalogSubsystem::Get(World) : nullptr;
		if (!Catalog)
		{
			Ar.Log(TEXT("afl.Catalog.TypeLint - no catalog subsystem (run inside PIE)."));
			return;
		}
		struct FRule { const TCHAR* Prefix; EAFLCosmeticType A; EAFLCosmeticType B; };
		// B == A where the namespace maps to exactly one type; AFL.Weapon.* is genuinely overloaded.
		static const FRule Rules[] = {
			{ TEXT("AFL.Edge."),      EAFLCosmeticType::SkinColor_Edge,  EAFLCosmeticType::SkinColor_Edge },
			{ TEXT("AFL.Body."),      EAFLCosmeticType::SkinColor_Body,  EAFLCosmeticType::SkinColor_Body },
			{ TEXT("AFL.Facemask."),  EAFLCosmeticType::Facemask,        EAFLCosmeticType::Facemask },
			{ TEXT("AFL.Character."), EAFLCosmeticType::Character,       EAFLCosmeticType::Character },
			{ TEXT("AFL.Team."),      EAFLCosmeticType::Team,            EAFLCosmeticType::Team },
			{ TEXT("AFL.Beam."),      EAFLCosmeticType::Beam,            EAFLCosmeticType::Beam },
			{ TEXT("AFL.Finish."),    EAFLCosmeticType::Finish,          EAFLCosmeticType::Finish },
			{ TEXT("AFL.Bundle."),    EAFLCosmeticType::Bundle,          EAFLCosmeticType::Bundle },
			{ TEXT("AFL.Emblem."),    EAFLCosmeticType::Emblem,          EAFLCosmeticType::Emblem },
			{ TEXT("AFL.Helmet."),    EAFLCosmeticType::Helmet,          EAFLCosmeticType::Helmet },
			{ TEXT("AFL.Ability."),   EAFLCosmeticType::AbilityCosmetic, EAFLCosmeticType::AbilityCosmetic },
			{ TEXT("AFL.Weapon."),    EAFLCosmeticType::Weapon,          EAFLCosmeticType::WeaponAccessory },
		};
		const UEnum* TypeEnum = StaticEnum<EAFLCosmeticType>();
		int32 Checked = 0, Mismatch = 0, Unmapped = 0, InvalidRows = 0;
		TMap<FString, int32> ByPrefix;
		for (int32 T = 0; T < (TypeEnum ? TypeEnum->NumEnums() : 0); ++T)
		{
			TArray<const FAFLCatalogEntry*> OfType;
			Catalog->GetEntriesByType(static_cast<EAFLCosmeticType>(TypeEnum->GetValueByIndex(T)), OfType);
			for (const FAFLCatalogEntry* R : OfType)
			{
				if (!R) { continue; }
				const FString Id = R->CosmeticId.ToString();
				const FRule* Hit = nullptr;
				for (const FRule& Rule : Rules)
				{
					if (Id.StartsWith(Rule.Prefix, ESearchCase::IgnoreCase)) { Hit = &Rule; break; }
				}
				++Checked;
				if (R->Type == EAFLCosmeticType::Invalid) { ++InvalidRows; }
				if (!Hit) { ++Unmapped; continue; }
				if (R->Type != Hit->A && R->Type != Hit->B)
				{
					++Mismatch;
					FString Pfx(Hit->Prefix);
					ByPrefix.FindOrAdd(Pfx)++;
					Ar.Logf(TEXT("  MISMATCH %s  type=%s  expected=%s"), *Id,
						*TypeEnum->GetNameStringByValue((int64)R->Type),
						*TypeEnum->GetNameStringByValue((int64)Hit->A));
				}
			}
		}
		FString Split;
		for (const TPair<FString, int32>& KV : ByPrefix) { Split += FString::Printf(TEXT("%s=%d "), *KV.Key, KV.Value); }
		Ar.Logf(TEXT("AFL_TEST[TYPELINT] checked=%d mismatch=%d unmapped=%d invalid=%d  %s"),
			Checked, Mismatch, Unmapped, InvalidRows, *Split);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[TYPELINT] checked=%d mismatch=%d unmapped=%d invalid=%d  %s"),
			Checked, Mismatch, Unmapped, InvalidRows, *Split);
	}

	// --- CC-3 BUILD PROOF -----------------------------------------------------------------------------
	// Drives the SAVED-BUILD path through the real RPCs, never by writing state directly: the point is to
	// prove that a build resolves INTO the one FAFLCosmeticSelection gameplay reads, and a direct write
	// would prove nothing about the resolve.
	//
	// WHAT WOULD FALSIFY THIS: after activating build N, the committed selection must carry build N's
	// colour. If both builds report the same colour, or the colour matches neither, the resolve is broken.
	// The two builds are deliberately FAR APART in colour so "wrong build active" and "no build active"
	// are distinguishable from each other, not merely from success.
	void HandleAFLCreatorBuildProbe(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Creator.BuildProbe - run inside PIE.")); return; }
		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout) { Ar.Log(TEXT("afl.Creator.BuildProbe - no loadout component.")); return; }

		const FString Mode = Args.Num() > 0 ? Args[0].ToLower() : FString();

		if (Mode == TEXT("seed"))
		{
			// Two builds, far apart. Both continuum, so neither needs an entitlement the stub would fake.
			auto MakeBuild = [Loadout](const TCHAR* Name, const FLinearColor& Body, const FLinearColor& Edge, const FLinearColor& Glow)
			{
				FAFLCreatorBuild B;
				B.DisplayName  = Name;
				B.BaseSelection = Loadout->GetSelection();
				B.BodyChannel  = FAFLChannelValue::MakeContinuum(Body);
				B.EdgeChannel  = FAFLChannelValue::MakeContinuum(Edge);
				B.GlowChannel  = FAFLChannelValue::MakeContinuum(Glow);
				return B;
			};
			Loadout->ServerSaveBuild(MakeBuild(TEXT("ProbeA"),
				FLinearColor(0.90f, 0.05f, 0.60f), FLinearColor(1.00f, 0.35f, 0.00f), FLinearColor(0.00f, 1.00f, 0.55f)), INDEX_NONE);
			Loadout->ServerSaveBuild(MakeBuild(TEXT("ProbeB"),
				// NOT (0.05,0.90,0.80): that cyan is ALREADY a persisted selection value, so a client
				// reporting it could be echoing its own inherited state rather than this build -- the read
				// could not tell the two apart. Orange appears nowhere in persistence or the preset set.
				FLinearColor(1.00f, 0.45f, 0.00f), FLinearColor(0.20f, 0.20f, 1.00f), FLinearColor(1.00f, 1.00f, 0.10f)), INDEX_NONE);
			Ar.Log(TEXT("afl.Creator.BuildProbe - seeded 2 builds via ServerSaveBuild."));
			return;
		}
		if (Mode == TEXT("use") && Args.Num() > 1)
		{
			Loadout->ServerSetActiveBuild(FCString::Atoi(*Args[1]));
			return;
		}
		if (Mode == TEXT("pull"))
		{
			// CC-3.5 RELAUNCH PROOF. Pull must run SERVER-SIDE (authority-only, and only the server holds
			// the signer), so reach this player's component in the in-process dedicated-server world by
			// PlayerId -- the same route the kill and the lapse rule needed, for the same reason.
			int32 MyId = PS ? PS->GetPlayerId() : -1;
			UAFLCosmeticLoadoutComponent* SrvLoadout = nullptr;
			if (GEngine && MyId >= 0)
			{
				for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
				{
					UWorld* CW = Ctx.World();
					if (!CW || !CW->IsGameWorld() || CW->GetNetMode() != NM_DedicatedServer) { continue; }
					for (FConstPlayerControllerIterator It = CW->GetPlayerControllerIterator(); It; ++It)
					{
						APlayerController* SrvPC = It->Get();
						APlayerState* SrvPS = SrvPC ? SrvPC->PlayerState : nullptr;
						if (SrvPS && SrvPS->GetPlayerId() == MyId)
						{ SrvLoadout = SrvPS->FindComponentByClass<UAFLCosmeticLoadoutComponent>(); break; }
					}
					if (SrvLoadout) { break; }
				}
			}
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUILDSYNC] pull request myPid=%d srvLoadout=%s"),
				MyId, SrvLoadout ? TEXT("FOUND") : TEXT("MISSING"));
			if (SrvLoadout) { SrvLoadout->PullBuildsFromPersistence(); }
			return;
		}
		if (Mode == TEXT("lapse") && Args.Num() > 2)
		{
			// CC-4.2: drive the lapse rule with an explicit cap. The cap is a PARAMETER by design
			// (product intent owns the number), so the probe supplies it rather than deriving one.
			//
			// MUST RUN SERVER-SIDE. ApplyLapseRule is authority-only, so calling it on this client is a
			// SILENT no-op -- measured: zero LAPSE emits and readOnly=0 everywhere, which reads exactly
			// like "nothing needed locking". Same defect shape as the CC-2.2 client-side kill. Reach the
			// dedicated-server world in-process and act on THIS player's server-side component, matched
			// by replicated PlayerId -- deliberately NOT a Server RPC, which would let a client hand
			// itself a slot cap.
			const int32 Cap = FCString::Atoi(*Args[1]);
			const bool bHeld = FCString::Atoi(*Args[2]) != 0;
			int32 MyId = PS ? PS->GetPlayerId() : -1;
			UAFLCosmeticLoadoutComponent* SrvLoadout = nullptr;
			if (GEngine && MyId >= 0)
			{
				for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
				{
					UWorld* CW = Ctx.World();
					if (!CW || !CW->IsGameWorld() || CW->GetNetMode() != NM_DedicatedServer) { continue; }
					for (FConstPlayerControllerIterator It = CW->GetPlayerControllerIterator(); It; ++It)
					{
						APlayerController* SrvPC = It->Get();
						APlayerState* SrvPS = SrvPC ? SrvPC->PlayerState : nullptr;
						if (SrvPS && SrvPS->GetPlayerId() == MyId)
						{
							SrvLoadout = SrvPS->FindComponentByClass<UAFLCosmeticLoadoutComponent>();
							break;
						}
					}
					if (SrvLoadout) { break; }
				}
			}
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[LAPSE] request cap=%d held=%d myPid=%d srvLoadout=%s"),
				Cap, bHeld ? 1 : 0, MyId, SrvLoadout ? TEXT("FOUND") : TEXT("MISSING"));
			if (SrvLoadout) { SrvLoadout->ApplyLapseRule(Cap, bHeld); }
			return;
		}

		// READ: report the build set AND the committed selection together, so "which build is active" and
		// "what actually got committed" can be compared rather than assumed equal.
		const FAFLCreatorBuildSet& Set = Loadout->GetBuildSet();
		const FAFLCosmeticSelection& Sel = Loadout->GetSelection();
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[BUILDPROBE] builds=%d active=%d creatorOn=%d editLocked=%d selBody=(%.4f,%.4f,%.4f)"),
			Set.Builds.Num(), Set.ActiveBuildIndex, Sel.bUseCreatorColors ? 1 : 0,
			Loadout->IsContinuumEditingLocked() ? 1 : 0,
			Sel.CreatorBodyColor.R, Sel.CreatorBodyColor.G, Sel.CreatorBodyColor.B);
		for (int32 i = 0; i < Set.Builds.Num(); ++i)
		{
			const FAFLCreatorBuild& B = Set.Builds[i];
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[BUILDPROBE]   [%d] %s bodySrc=%d body=(%.4f,%.4f,%.4f) readOnly=%d"),
				i, *B.DisplayName, (int32)B.BodyChannel.Source,
				B.BodyChannel.Resolved.R, B.BodyChannel.Resolved.G, B.BodyChannel.Resolved.B,
				B.bReadOnly ? 1 : 0);
		}
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCreatorBuildProbeCmd(
		TEXT("afl.Creator.BuildProbe"),
		TEXT("CC-3 proof: 'seed' saves two far-apart continuum builds through ServerSaveBuild; 'use <n>' activates one through ServerSetActiveBuild; no args reports the build set AND the committed selection so the resolve can be compared, not assumed."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCreatorBuildProbe));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCatalogTypeLintCmd(
		TEXT("afl.Catalog.TypeLint"),
		TEXT("CC-X17: report catalog rows whose Type disagrees with their id namespace -- the detectable subset of the Type-default trap. Catches disagreement, NOT wrongness: a row genuinely mistyped within its own namespace is a provenance question no value read answers."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCatalogTypeLint));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetFacemaskCmd(
		TEXT("afl.Cosmetic.SetFacemask"),
		TEXT("Facemask selection seam: client-issued PURE caller of ServerSetCosmeticSelection (sets FacemaskId). Runtime equip path -> slot-1 material swap + finish re-layer. Ids resolve against the CATALOG (exact row id, or a bare name unique to one row; ambiguous or unknown = loud error, nothing equipped). Usage: afl.Cosmetic.SetFacemask <AFL.Facemask.Flag.Japan | AFL.Facemask.IroVisor | none | verify>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetFacemask));

	// ─── Phase 0 identity seam: afl.Cosmetic.SetIdentity <TeamName> ──────────────
	// Sets the player's IDENTITY (Team axis) so the CharacterId->robot-part selector (UAFLCharacterPartSelector
	// Component) resolves a DIFFERENT body per player. The proven SetEdge cheat above only ever pins
	// IdentityType=Team/TeamId=AFL.Team.ARIA -- this is the missing instrument to exercise per-player
	// differentiation (e.g. one player IRONICS, another SCARLETT). PURE: client-issued; the server validates +
	// commits + replicates, and the controller selector re-resolves the body on the next possession.
	void HandleAFLCosmeticSetIdentity(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetIdentity — usage: afl.Cosmetic.SetIdentity <ARIA|IRONICS|SCARLETT|MAKHIAVELLI|AP-9|MOB-FIGAZ> (or full AFL.Team.<Name>)."));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetIdentity — no game world (run inside PIE)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetIdentity — no UAFLCosmeticLoadoutComponent on the local player's PlayerState."));
			return;
		}

		FString IdStr = Args[0].TrimStartAndEnd();
		if (!IdStr.StartsWith(TEXT("AFL.Team."), ESearchCase::IgnoreCase))
		{
			IdStr = FString::Printf(TEXT("AFL.Team.%s"), *IdStr);
		}
		const FName TeamId(*IdStr);

		FAFLCosmeticSelection Request = Loadout->GetSelection();
		Request.IdentityType = EAFLIdentityType::Team;
		Request.TeamId = TeamId;

		Loadout->ServerSetCosmeticSelection(Request); // PURE: client-issued; server does the rest.

		Ar.Logf(TEXT("afl.Cosmetic.SetIdentity — client issued ServerSetCosmeticSelection(identity=Team/%s). Re-possess (or it applies on next possession) to see the body swap."),
			*TeamId.ToString());
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetIdentityCmd(
		TEXT("afl.Cosmetic.SetIdentity"),
		TEXT("Phase 0 identity seam: set the player's Team identity so the body selector resolves a different robot. Usage: afl.Cosmetic.SetIdentity <ARIA|IRONICS|SCARLETT|MAKHIAVELLI|AP-9|MOB-FIGAZ> (or full AFL.Team.<Name>)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetIdentity));

	// ─── Phase 1 CHARACTER axis: afl.Cosmetic.SetCharacter <CharacterName> ───────
	// Sibling of SetIdentity, one enum-value over: sets the CHARACTER axis (IdentityType=Character + CharacterId)
	// instead of Team, via the SAME ServerSetCosmeticSelection (which already type-branches Character -- #43).
	// The proven SetIdentity (Team) cheat is UNTOUCHED so both axes stay independently testable (the parallel
	// Character/Team model). The body selector reads GetActiveIdentityId(), which returns CharacterId when the
	// type is Character -> forcing a Character selection here makes the resolver resolve an AFL.Character.* key
	// (e.g. Big Sixx) in the current solo-context PIE. (Match-type auto-switch = the Phase-2 follow-up.)
	void HandleAFLCosmeticSetCharacter(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetCharacter — usage: afl.Cosmetic.SetCharacter <BigSixx|...> (or full AFL.Character.<Name>)."));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetCharacter — no game world (run inside PIE)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetCharacter — no UAFLCosmeticLoadoutComponent on the local player's PlayerState."));
			return;
		}

		FString IdStr = Args[0].TrimStartAndEnd();
		if (!IdStr.StartsWith(TEXT("AFL.Character."), ESearchCase::IgnoreCase))
		{
			IdStr = FString::Printf(TEXT("AFL.Character.%s"), *IdStr);
		}
		const FName CharacterId(*IdStr);

		FAFLCosmeticSelection Request = Loadout->GetSelection();
		Request.IdentityType = EAFLIdentityType::Character;
		Request.CharacterId = CharacterId;

		Loadout->ServerSetCosmeticSelection(Request); // PURE: client-issued; server does the rest.

		Ar.Logf(TEXT("afl.Cosmetic.SetCharacter — client issued ServerSetCosmeticSelection(identity=Character/%s). Re-possess (or it applies on next possession) to see the body swap."),
			*CharacterId.ToString());
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetCharacterCmd(
		TEXT("afl.Cosmetic.SetCharacter"),
		TEXT("Phase 1 Character axis: set the player's Character identity (IdentityType=Character) so the body selector resolves an AFL.Character.* robot (e.g. Big Sixx). Usage: afl.Cosmetic.SetCharacter <BigSixx> (or full AFL.Character.<Name>)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetCharacter));

	// (RETIRED: afl.Cosmetic.SetHelmet + the helmet part-path it drove. The facemask is now a material
	//  reskin -- a slot-base-MI cosmetic on the proven logo channel (MI_AFL_FaceMask_Pink) -- not a
	//  CharacterPart add. See the facemask commit; the part-path apparatus was deleted as a non-problem.)

	// ─── PANEL ADDRESS-BOOK WATCH: afl.Cosmetic.SetParam / SetParamScalar ─────────
	//
	// Reusable instrument (NOT a throwaway): poke ONE named material param on the local player's robot
	// part actor(s)' LIVE MIDs, so the operator can watch which physical region the param paints and
	// complete the param->region address book in ONE PIE pass. Pokes the MID in isolation (no edge-apply-
	// vs-brand-default confound), works on any param at any value, and stays as infrastructure for the
	// facemask + future skin-SKU authoring. The robot body is an AAFLCharacterPartActor child-actor on the
	// pawn (spawned by Lyra's CharacterParts component); we find it via the pawn's UChildActorComponents.
	// LOCAL/cosmetic-only: MIDs are client-side visuals -> run it in the window you're watching.
	//
	// NOTE on the map: EdgeGlowMagnitude is 0.0 natively (rim OFF) -> to see the edge region, first
	// `afl.Cosmetic.SetParamScalar EdgeGlowMagnitude 1` THEN `afl.Cosmetic.SetParam EdgeGlowColor 1 0 0`.
	// (The blue we saw earlier was the bolted-on B_AFL_Helmet_Visor part being retired, NOT the native edge.)

	// Collect the local player's robot body part actors (AAFLCharacterPartActor children on the pawn).
	void GatherPlayerPartActors(UWorld* World, TArray<AAFLCharacterPartActor*>& Out)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			return;
		}
		TArray<UChildActorComponent*> ChildComps;
		Pawn->GetComponents<UChildActorComponent>(ChildComps);
		for (UChildActorComponent* CAC : ChildComps)
		{
			if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr))
			{
				Out.AddUnique(Part);
			}
		}
	}

	// ---- CC-1.2-P · CHASSIS EMBLEM PROBE (log-only verdict) ------------------------------------------
	// One command, four AFL_TEST markers, no visual observation. Spawns a body through the PART-MAP RESOLVER
	// (not a hardcoded path), applies a KNOWN TAGGED colour preset, then READS THE DECAL MATERIAL BACK.
	// P4 is the verdict line: the write log (AFLCharacterPartActor.cpp:322) proves only that the write was
	// ISSUED -- it cannot distinguish "wrote and stuck" from "wrote then something overwrote it" or "the MID
	// we wrote is not the instance the decal renders". A readback distinguishes all three.
	//
	// Why the 1s timer: AFLPossessAs_Apply swaps the body through the CharacterParts fast-array and the part is
	// a CHILD ACTOR that does not exist on the same frame. Probing inline would report P3=0 for a purely timing
	// reason and send the verdict down the "decal never attached" branch.
	static const TCHAR* kEmblemProbePreset =
		TEXT("/Game/BagMan/Characters/Cosmetics/Finishes/DA_AFL_Finish_Blue_Ironics.DA_AFL_Finish_Blue_Ironics");

	void AFLEmblemProbe_Run(UWorld* World, const FString& PresetPath)
	{
		static const FName NeonColorParam(TEXT("NeonColor"));

		TArray<AAFLCharacterPartActor*> Parts;
		GatherPlayerPartActors(World, Parts);
		if (Parts.Num() == 0)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[P2] preset applied: <none> tag=<none> valid=false  (NO PART ACTOR on pawn -- probe aborted)"));
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[P3] decal component count on spawned actor = 0  (no part actor)"));
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[P4] readback DecalMID NeonColor = <no part actor>"));
			return;
		}

		// P2 -- the preset, EXPLICIT and TAGGED. Never DA_AFL_Finish_GlossBlack (deliberately untagged,
		// AFLCharacterPartActor.cpp:158-159) -- an untagged preset leaves bIdentityResolved false and the
		// emblem block at :291 is skipped, which would look identical to a failed write.
		UAFLSkinColorAsset* Preset = LoadObject<UAFLSkinColorAsset>(nullptr, *PresetPath);
		const FGameplayTag PresetTag = Preset ? Preset->GetColorIdentityTag() : FGameplayTag();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[P2] preset applied: %s tag=%s valid=%s"),
			*GetNameSafe(Preset),
			PresetTag.IsValid() ? *PresetTag.ToString() : TEXT("<invalid>"),
			PresetTag.IsValid() ? TEXT("true") : TEXT("false"));

		for (AAFLCharacterPartActor* Part : Parts)
		{
			if (!IsValid(Part))
			{
				continue;
			}

			if (Preset)
			{
				Part->ApplySkinColor(Preset); // the write under test -- the :291 emblem block runs inside this
			}

			// P3 -- decals on the SPAWNED actor (the runtime instance, not the CDO/SCS template).
			TArray<UDecalComponent*> Decals;
			Part->GetComponents<UDecalComponent>(Decals);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[P3] decal component count on spawned actor = %d  (part=%s)"),
				Decals.Num(), *Part->GetName());

			// P4 -- THE VERDICT LINE.
			for (UDecalComponent* Decal : Decals)
			{
				if (!IsValid(Decal))
				{
					continue;
				}
				UMaterialInterface* Mat = Decal->GetDecalMaterial();
				FLinearColor Tone(-1.f, -1.f, -1.f, -1.f);
				bool bRead = false;
				if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mat))
				{
					Tone = MID->K2_GetVectorParameterValue(NeonColorParam);
					bRead = true;
				}
				else if (Mat)
				{
					// Not a MID -> the part never re-created one for this decal; report the MIC's baked value.
					bRead = Mat->GetVectorParameterValue(FMaterialParameterInfo(NeonColorParam), Tone);
				}
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[P4] readback DecalMID NeonColor = %s  (decal=%s mat=%s class=%s read=%s)"),
					*Tone.ToString(), *Decal->GetName(), *GetNameSafe(Mat),
					Mat ? *Mat->GetClass()->GetName() : TEXT("null"),
					bRead ? TEXT("ok") : TEXT("FAILED"));
			}
		}
	}

	void HandleAFLChassisEmblemProbe(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Chassis.EmblemProbe - run inside PIE."));
			return;
		}

		const FName IdentityId = (Args.Num() >= 1) ? FName(*Args[0].TrimStartAndEnd()) : FName(TEXT("AFL.Chassis.Creator"));
		const FString PresetPath = (Args.Num() >= 2) ? Args[1].TrimStartAndEnd() : FString(kEmblemProbePreset);

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[P1] chassis spawn requested  (identity=%s preset=%s)"),
			*IdentityId.ToString(), *PresetPath);

		// PART-MAP RESOLVER -- identity id -> robot body CharacterPart class. Deliberately NOT a hardcoded body
		// path: if the chassis is unmapped, that is itself the finding and must surface as a P1 failure.
		UAFLCharacterPartMap* PartMap = LoadObject<UAFLCharacterPartMap>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/SkinColors/DA_AFL_CharacterPartMap.DA_AFL_CharacterPartMap"));
		if (!PartMap)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[P1] FAILED - DA_AFL_CharacterPartMap did not load"));
			Ar.Log(TEXT("afl.Chassis.EmblemProbe - part map failed to load."));
			return;
		}

		const TSoftClassPtr<AActor> SoftPart = PartMap->ResolveCharacterPart(IdentityId);
		UClass* PartClass = SoftPart.IsNull() ? nullptr : SoftPart.LoadSynchronous();
		if (!PartClass)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[P1] FAILED - '%s' unmapped in the part map, or its class failed to load"),
				*IdentityId.ToString());
			Ar.Logf(TEXT("afl.Chassis.EmblemProbe - '%s' has no part-map row."), *IdentityId.ToString());
			return;
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[P1] resolved class=%s"), *GetNameSafe(PartClass));

		FString Err;
		if (!AFLPossessAs_Apply(World, PartClass, Err))
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[P1] FAILED - possess/apply: %s"), *Err);
			Ar.Logf(TEXT("afl.Chassis.EmblemProbe - %s"), *Err);
			return;
		}

		FTimerHandle ProbeTimer;
		const FString CapturedPreset = PresetPath;
		World->GetTimerManager().SetTimer(ProbeTimer,
			FTimerDelegate::CreateLambda([World, CapturedPreset]()
			{
				AFLEmblemProbe_Run(World, CapturedPreset);
			}),
			1.0f, false);

		Ar.Log(TEXT("afl.Chassis.EmblemProbe - armed; P2-P4 emit in ~1s. Then close PIE and read Saved/Logs for AFL_TEST[P1..P4]."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLChassisEmblemProbeCmd(
		TEXT("afl.Chassis.EmblemProbe"),
		TEXT("CC-1.2-P log-only probe: spawn an identity's body via the PART-MAP resolver, apply a TAGGED colour preset, then READ BACK the chest-emblem decal's NeonColor. Emits AFL_TEST[P1..P4]. Usage: afl.Chassis.EmblemProbe [IdentityId] [PresetObjectPath] (defaults: AFL.Chassis.Creator + DA_AFL_Finish_Blue_Ironics)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLChassisEmblemProbe));

	// ---- CC-1.1-P · SLOT-1 RESTORE PROOF ------------------------------------------------------------
	// Q1..Q4 are ALL direct readbacks of Mesh->GetMaterial(1) -- never a log of what was passed in.
	//
	// Q2 without touching the class: AuthoredSlot1Material is PROTECTED (AFLCharacterPartActor.h:177), so a
	// cheat cannot read it. It does not need to. ApplyFacemask(nullptr) RESTORES that captured value into
	// slot 1 (:390-394), so a readback taken immediately after an un-equip IS the capture -- observed, not
	// inferred. That is exactly what Q2 reports, and it is why Q2 runs BEFORE anything is equipped.
	//
	// The capture itself (:380) already happened during BeginPlay -- ApplyFacemask runs at :89, before
	// ApplySkinColor at :95 -- so this probe cannot disturb it; it can only observe what was captured.
	//
	// Calling ApplyFacemask directly is the faithful exercise: the replicated path
	// (SetFacemask -> OnRep -> ReapplyFacemaskToAllParts) funnels into this same call, and the capture/restore
	// under test lives entirely inside it.
	static const TCHAR* kSlot1ProbeFacemask =
		TEXT("/Game/BagMan/Materials/Instances/MI_AFL_FaceMask_JollyRoger.MI_AFL_FaceMask_JollyRoger");

	static USkeletalMeshComponent* AFLSlot1_FindVisorMesh(AAFLCharacterPartActor* Part)
	{
		// Mirror ApplyFacemask's own filter (:370): the mesh must actually HAVE a slot 1.
		TArray<USkeletalMeshComponent*> Meshes;
		Part->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* M : Meshes)
		{
			if (IsValid(M) && M->GetNumMaterials() > 1)
			{
				return M;
			}
		}
		return nullptr;
	}

	static FString AFLSlot1_Read(USkeletalMeshComponent* Mesh)
	{
		if (!IsValid(Mesh)) { return TEXT("<no-mesh>"); }
		UMaterialInterface* M = Mesh->GetMaterial(1);
		if (!M) { return TEXT("null"); }
		// A MID name alone hides which base it wraps -- report the parent too, since the whole question is
		// WHICH base slot 1 resolves to.
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M))
		{
			return FString::Printf(TEXT("%s [MID parent=%s]"), *M->GetName(), *GetNameSafe(MID->Parent));
		}
		return FString::Printf(TEXT("%s [%s]"), *M->GetName(), *M->GetClass()->GetName());
	}

	void AFLSlot1Probe_Run(UWorld* World, const FString& FacemaskPath)
	{
		TArray<AAFLCharacterPartActor*> Parts;
		GatherPlayerPartActors(World, Parts);
		if (Parts.Num() == 0)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q1] NO PART ACTOR on pawn -- probe aborted"));
			return;
		}

		UMaterialInstanceConstant* Mask = LoadObject<UMaterialInstanceConstant>(nullptr, *FacemaskPath);
		if (!Mask)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q3] FACEMASK FAILED TO LOAD: %s"), *FacemaskPath);
		}

		for (AAFLCharacterPartActor* Part : Parts)
		{
			if (!IsValid(Part)) { continue; }

			USkeletalMeshComponent* Mesh = AFLSlot1_FindVisorMesh(Part);
			if (!Mesh)
			{
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q1] part=%s has NO slot-1 mesh -- skipped"), *Part->GetName());
				continue;
			}

			// Q1 -- the as-spawned state (BeginPlay's ApplyFacemask + ApplySkinColor have already run).
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q1] chassis spawned, slot[1] material at BeginPlay = %s  (part=%s)"),
				*AFLSlot1_Read(Mesh), *Part->GetName());

			// Q2 -- un-equip FIRST: restores AuthoredSlot1Material into slot 1, so this readback IS the capture.
			Part->ApplyFacemask(nullptr, nullptr);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q2] AuthoredSlot1Material captured = %s"),
				*AFLSlot1_Read(Mesh));

			// Q3 -- equip the probe facemask (unique-body parts auto-derive the _V visor variant internally).
			Part->ApplyFacemask(Mask, nullptr);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q3] facemask equipped: slot[1] = %s  (requested=%s)"),
				*AFLSlot1_Read(Mesh), *GetNameSafe(Mask));

			// Q4 -- THE VERDICT. Un-equip, then read the MESH back.
			Part->ApplyFacemask(nullptr, nullptr);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q4] facemask UNEQUIPPED: slot[1] = %s"),
				*AFLSlot1_Read(Mesh));
		}
	}

	void HandleAFLChassisSlot1Probe(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Chassis.Slot1Probe - run inside PIE."));
			return;
		}

		const FName IdentityId = (Args.Num() >= 1) ? FName(*Args[0].TrimStartAndEnd()) : FName(TEXT("AFL.Chassis.Creator"));
		const FString MaskPath = (Args.Num() >= 2) ? Args[1].TrimStartAndEnd() : FString(kSlot1ProbeFacemask);

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q0] slot-1 probe requested  (identity=%s facemask=%s)"),
			*IdentityId.ToString(), *MaskPath);

		UAFLCharacterPartMap* PartMap = LoadObject<UAFLCharacterPartMap>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/SkinColors/DA_AFL_CharacterPartMap.DA_AFL_CharacterPartMap"));
		if (!PartMap)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q0] FAILED - DA_AFL_CharacterPartMap did not load"));
			Ar.Log(TEXT("afl.Chassis.Slot1Probe - part map failed to load."));
			return;
		}

		const TSoftClassPtr<AActor> SoftPart = PartMap->ResolveCharacterPart(IdentityId);
		UClass* PartClass = SoftPart.IsNull() ? nullptr : SoftPart.LoadSynchronous();
		if (!PartClass)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q0] FAILED - '%s' unmapped or class failed to load"),
				*IdentityId.ToString());
			Ar.Logf(TEXT("afl.Chassis.Slot1Probe - '%s' has no part-map row."), *IdentityId.ToString());
			return;
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q0] resolved class=%s"), *GetNameSafe(PartClass));

		FString Err;
		if (!AFLPossessAs_Apply(World, PartClass, Err))
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[Q0] FAILED - possess/apply: %s"), *Err);
			Ar.Logf(TEXT("afl.Chassis.Slot1Probe - %s"), *Err);
			return;
		}

		// Same 1s delay as the emblem probe: the part is a CHILD ACTOR that does not exist on the possess frame.
		FTimerHandle ProbeTimer;
		const FString CapturedMask = MaskPath;
		World->GetTimerManager().SetTimer(ProbeTimer,
			FTimerDelegate::CreateLambda([World, CapturedMask]()
			{
				AFLSlot1Probe_Run(World, CapturedMask);
			}),
			1.0f, false);

		Ar.Log(TEXT("afl.Chassis.Slot1Probe - armed; Q1-Q4 emit in ~1s. Then close PIE and read Saved/Logs for AFL_TEST[Q1..Q4]."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLChassisSlot1ProbeCmd(
		TEXT("afl.Chassis.Slot1Probe"),
		TEXT("CC-1.1-P log-only probe: spawn a body via the PART-MAP resolver, then DIRECTLY read back Mesh->GetMaterial(1) across un-equip / equip / un-equip to prove what slot 1 restores to. Emits AFL_TEST[Q1..Q4]; Q4 is the verdict. Usage: afl.Chassis.Slot1Probe [IdentityId] [FacemaskMICPath] (defaults: AFL.Chassis.Creator + MI_AFL_FaceMask_JollyRoger)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLChassisSlot1Probe));

	// ---- CC-2.1-P · CREATOR COLOUR OVERLAY PROOF (dedicated server + 2 clients) -----------------------
	// Drives the REAL funnel: ServerSetCosmeticSelection (client->server RPC), never SetSkinColor -- so the
	// server-side ClampToNeon (AFLCosmeticLoadoutComponent.cpp:125) and the commit are both exercised.
	//
	// R3/R4 read the MID on whichever machine the command runs on, and iterate EVERY AAFLCharacterPartActor in
	// the world (not just the local player's), logging each one's owning PlayerState -- that is how client B
	// reads client A's robot. ActiveColorOverride is protected, so the MID params ARE the proof, not the struct.
	//
	// Colours: Body is DELIBERATELY out of the neon gamut (desaturated + near-black: S~0.09 < 0.55 floor,
	// V~0.22 < 0.45 floor) so BOTH clamp floors fire and R2 != R1 is a real measurement, not an assumption.
	// Edge/Glow are already in gamut (S=1, V=1) and should pass through hue-preserved.
	static const FLinearColor kCreatorBody(0.20f, 0.20f, 0.22f, 1.0f); // OUT OF GAMUT -> clamp must change it
	static const FLinearColor kCreatorEdge(1.00f, 0.35f, 0.00f, 1.0f); // vivid orange, in gamut
	static const FLinearColor kCreatorGlow(0.00f, 1.00f, 0.55f, 1.0f); // vivid spring green, in gamut

	static FString AFLCreator_NetTag(UWorld* World)
	{
		if (!World) { return TEXT("?"); }
		switch (World->GetNetMode())
		{
		case NM_DedicatedServer: return TEXT("DEDSRV");
		case NM_ListenServer:    return TEXT("LISTEN");
		case NM_Client:          return TEXT("CLIENT");
		case NM_Standalone:      return TEXT("STANDALONE");
		default:                 return TEXT("?");
		}
	}

	static FString AFLCreator_C(const FLinearColor& C)
	{
		return FString::Printf(TEXT("(%.4f,%.4f,%.4f,%.4f)"), C.R, C.G, C.B, C.A);
	}

	// Read the three creator channels off every MID on every part actor in THIS world.
	static void AFLCreator_ReadMIDs(UWorld* World, const TCHAR* Marker)
	{
		static const FName NTeam(TEXT("TeamColor"));
		static const FName NEdge(TEXT("EdgeGlowColor"));
		static const FName NEmis(TEXT("EmissiveColor"));

		int32 PartCount = 0;
		for (TActorIterator<AAFLCharacterPartActor> It(World); It; ++It)
		{
			AAFLCharacterPartActor* Part = *It;
			if (!IsValid(Part)) { continue; }
			++PartCount;

			// Whose robot is this? On client B, A's part is owned by A's PlayerState.
			FString Owner = TEXT("<no-owner>");
			if (AActor* Outer = Part->GetOwner())
			{
				Owner = Outer->GetName();
				if (APawn* P = Cast<APawn>(Outer))
				{
					if (APlayerState* PS = P->GetPlayerState()) { Owner += TEXT("/PS=") + PS->GetPlayerName(); }
				}
			}

			TArray<USkeletalMeshComponent*> Meshes;
			Part->GetComponents<USkeletalMeshComponent>(Meshes);
			for (USkeletalMeshComponent* Mesh : Meshes)
			{
				if (!IsValid(Mesh)) { continue; }
				for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
				{
					UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot));
					if (!MID) { continue; } // not a MID -> nothing was pushed onto this slot
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[%s] [%s] part=%s owner=%s slot=%d mid=%s TeamColor=%s EdgeGlowColor=%s EmissiveColor=%s"),
						Marker, *AFLCreator_NetTag(World), *Part->GetName(), *Owner, Slot, *MID->GetName(),
						*AFLCreator_C(MID->K2_GetVectorParameterValue(NTeam)),
						*AFLCreator_C(MID->K2_GetVectorParameterValue(NEdge)),
						*AFLCreator_C(MID->K2_GetVectorParameterValue(NEmis)));
				}
			}
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[%s] [%s] part actors iterated = %d"),
			Marker, *AFLCreator_NetTag(World), PartCount);
	}

	// Read the COMMITTED (post-clamp) creator fields back off the replicated Selection.
	static void AFLCreator_ReadCommitted(UWorld* World, const TCHAR* Marker)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[%s] [%s] NO loadout component -- committed read unavailable"),
				Marker, *AFLCreator_NetTag(World));
			return;
		}
		const FAFLCosmeticSelection Sel = Loadout->GetSelection();
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[%s] [%s] committed colours after clamp: bUseCreatorColors=%d Body=%s Edge=%s Glow=%s"),
			Marker, *AFLCreator_NetTag(World), (int32)Sel.bUseCreatorColors,
			*AFLCreator_C(Sel.CreatorBodyColor), *AFLCreator_C(Sel.CreatorEdgeColor), *AFLCreator_C(Sel.CreatorGlowColor));
	}

	void HandleAFLCreatorColorProbe(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Creator.ColorProbe - run inside PIE."));
			return;
		}

		// CC-2.1-I2 MODES. The old probe hardcoded one triple, so once persistence restored it the set wrote
		// IDENTICAL values -> no replication delta -> no OnRep -> nothing applied. The
		// instrument could not exercise the path it exists to test. Colours are now ARGUMENTS, and 'clear' gives
		// a guaranteed delta for the next set while exercising the turn-off path. Randomising instead would have
		// manufactured a delta every run and never tested the ALREADY-SET case -- which is the shipping case.
		// NO SILENT DEFAULT. This command previously ran SET with no args; I2 briefly made bare = READ, which
		// silently reinterpreted an invocation the operator already had muscle memory for -- the exact class of
		// change that costs a session. A bare command is now a USAGE ERROR that does nothing.
		if (Args.Num() == 0)
		{
			Ar.Log(TEXT("afl.Creator.ColorProbe requires a MODE (no default -- nothing was done)."));
			Ar.Log(TEXT("  read                                  -- readback only: R2 (+PROVENANCE) / R4"));
			Ar.Log(TEXT("  clear                                 -- bUseCreatorColors->0 via the real funnel; guarantees the NEXT set is a delta"));
			Ar.Log(TEXT("  set <bR> <bG> <bB> [eR eG eB gR gG gB] -- colours from args; body low-S/V measures the clamp"));
			return;
		}
		const FString Mode = Args[0].TrimStartAndEnd().ToLower();

		if (Mode == TEXT("read"))
		{
			// READ mode: NO commit, pure readback.
			AFLCreator_ReadCommitted(World, TEXT("R2"));
			AFLCreator_ReadMIDs(World, TEXT("R4"));
			Ar.Log(TEXT("afl.Creator.ColorProbe read - R2/R4 emitted (no commit)."));
			return;
		}

		const bool bClear = (Mode == TEXT("clear"));
		const bool bSet   = (Mode == TEXT("set"));
		if (!bClear && !bSet)
		{
			Ar.Log(TEXT("usage: afl.Creator.ColorProbe read | clear | set <bR> <bG> <bB> [eR eG eB gR gG gB]"));
			return;
		}
		if (bSet && Args.Num() < 4)
		{
			Ar.Log(TEXT("usage: afl.Creator.ColorProbe set <bR> <bG> <bB> [eR eG eB gR gG gB]  (values 0..1; body deliberately out-of-gamut measures the clamp)"));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			Ar.Log(TEXT("afl.Creator.ColorProbe - no UAFLCosmeticLoadoutComponent on the local PlayerState."));
			return;
		}

		// Colours come from ARGS (no hardcoded triple). Supplying only the body triple leaves edge/glow at the
		// in-gamut control values, so the pass-through control is preserved; the log states which were defaulted.
		FLinearColor ReqBody = kCreatorBody, ReqEdge = kCreatorEdge, ReqGlow = kCreatorGlow;
		bool bEdgeGlowDefaulted = true;
		if (bSet)
		{
			ReqBody = FLinearColor(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]), 1.0f);
			if (Args.Num() >= 10)
			{
				ReqEdge = FLinearColor(FCString::Atof(*Args[4]), FCString::Atof(*Args[5]), FCString::Atof(*Args[6]), 1.0f);
				ReqGlow = FLinearColor(FCString::Atof(*Args[7]), FCString::Atof(*Args[8]), FCString::Atof(*Args[9]), 1.0f);
				bEdgeGlowDefaulted = false;
			}
		}

		// R0 -- what this run INHERITED, so a no-delta run is self-evident instead of being mistaken for a failure.
		const FAFLCosmeticSelection Prev = Loadout->GetSelection();
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[R0] [%s] mode=%s INHERITED: bUse=%d Body=%s Edge=%s Glow=%s"),
			*AFLCreator_NetTag(World), *Mode, (int32)Prev.bUseCreatorColors,
			*AFLCreator_C(Prev.CreatorBodyColor), *AFLCreator_C(Prev.CreatorEdgeColor), *AFLCreator_C(Prev.CreatorGlowColor));

		// R1 -- the RAW request, before the server sees it, plus whether it can possibly be a delta.
		if (bClear)
		{
			const bool bWouldDelta = (Prev.bUseCreatorColors != 0);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[R1] [%s] CLEAR requested (bUseCreatorColors -> 0) expectDelta=%d"),
				*AFLCreator_NetTag(World), bWouldDelta ? 1 : 0);
		}
		else
		{
			const bool bWouldDelta =
				(Prev.bUseCreatorColors == 0) ||
				!Prev.CreatorBodyColor.Equals(ReqBody) || !Prev.CreatorEdgeColor.Equals(ReqEdge) || !Prev.CreatorGlowColor.Equals(ReqGlow);
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[R1] [%s] requested colours = Body%s Edge%s Glow%s (edge/glow defaulted=%d) expectDelta-preClamp=%d"),
				*AFLCreator_NetTag(World), *AFLCreator_C(ReqBody), *AFLCreator_C(ReqEdge), *AFLCreator_C(ReqGlow),
				bEdgeGlowDefaulted ? 1 : 0, bWouldDelta ? 1 : 0);
		}

		// PURE CALLER of the real funnel (clear included -- it goes through ServerSetCosmeticSelection, never a
		// direct write). Seed the free identity if unset so _Validate passes (same shape as the other Set* cheats).
		FAFLCosmeticSelection Request = Prev;
		if (Request.GetActiveIdentityId() == NAME_None)
		{
			Request.IdentityType = EAFLIdentityType::Team;
			Request.TeamId = FName(TEXT("AFL.Team.IRONICS"));
		}
		if (bClear)
		{
			Request.bUseCreatorColors = 0;
		}
		else
		{
			Request.bUseCreatorColors = 1;
			Request.CreatorBodyColor  = ReqBody;
			Request.CreatorEdgeColor  = ReqEdge;
			Request.CreatorGlowColor  = ReqGlow;
		}

		Loadout->ServerSetCosmeticSelection(Request); // client -> server; clamp + commit happen there

		// Give the RPC + the Selection OnRep + the push a couple of seconds to land, then read back locally.
		FTimerHandle T;
		World->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([World]()
		{
			AFLCreator_ReadCommitted(World, TEXT("R2"));
			AFLCreator_ReadMIDs(World, TEXT("R3"));
		}), 2.0f, false);

		Ar.Log(TEXT("afl.Creator.ColorProbe - request sent; R0/R1 now, R2/R3 in ~2s. Run 'afl.Creator.ColorProbe read' on the OTHER client, and again after respawn for R4."));
	}

#if !UE_BUILD_SHIPPING
	// ---- ACCEPTANCE HARNESS (dev-only; NEVER compiled into shipping) ------------------------------------
	// The bridge cannot inject console commands mid-PIE, so an unattended acceptance run needs the sequence to
	// drive itself. Arm `afl.Creator.AutoProbe 1` BEFORE starting PIE; the ticker waits for a CLIENT world with
	// a loadout, then reads back. #if-guarded because an unguarded static-init ticker would run at 1 Hz forever
	// in any config that compiles this TU.
	static int32 GAFLCreatorAutoProbe = 0;
static int32 GAFLCreatorPullOnly = 0;
static FAutoConsoleVariableRef CVarAFLCreatorPullOnly(TEXT("afl.Creator.PullOnly"),
	GAFLCreatorPullOnly, TEXT("CC-3.5 relaunch proof: 1 = pull builds from the backend instead of seeding."));
	static FAutoConsoleVariableRef GAFLCreatorAutoProbeCVar(
		TEXT("afl.Creator.AutoProbe"),
		GAFLCreatorAutoProbe,
		TEXT("Dev acceptance: 1 = on the first ready CLIENT world, auto-run the creator-overlay readback. Set BEFORE starting PIE. Self-disarms."),
		ECVF_Default);

	static bool GAFLCreatorAutoFired = false;

	static void AFLCreator_RunArgs(UWorld* World, const TArray<FString>& Args)
	{
		FOutputDeviceNull Null;
		HandleAFLCreatorColorProbe(Args, World, Null);
	}

	// CC-2.1-P2 A/B SEQUENCE. Roles by DISCOVERY ORDER, and the role assignment is LOGGED with the world +
	// PlayerState identity -- if discovery order is not stable across runs, a writer/observer mix-up would read
	// as an observer-leg failure. Cheap to state, expensive to misread.
	//   A (writer):   t=0 read | t=4 clear | t=8 read | t=12 set 0.2 0.2 0.22 | t=16 read
	//   B (observer): t=0 read |           | t=8 read |                       | t=16 read
	// Shared clock, so each read on B lines up with the read on A after the same delta.
	static void AFLCreator_AutoSequence(UWorld* World, int32 RoleIndex)
	{
		if (!World) { return; }
		const TCHAR* Role = (RoleIndex == 0) ? TEXT("A-WRITER") : TEXT("B-OBSERVER");
		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[ROLE] [%s] role=%s idx=%d world=%s PS=%s"),
			*AFLCreator_NetTag(World), Role, RoleIndex, *World->GetName(),
			PS ? *PS->GetName() : TEXT("<null>"));

		TWeakObjectPtr<UWorld> W = World;
		auto Fire = [W, Role](float Delay, TArray<FString> Args, const TCHAR* Step)
		{
			if (!W.IsValid()) { return; }
			FTimerHandle T;
			const FString StepStr(Step); const FString RoleStr(Role);
			W->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([W, Args, StepStr, RoleStr]()
			{
				if (!W.IsValid()) { return; }
				// PAWN IDENTITY is the RESPAWN DETECTOR: a changed pawn name between step 5 and step 6 IS the
				// respawn. Unchanged -> no death occurred in the window -> step 6 is NOT EXERCISED, not passed.
				APlayerController* StepPC = W->GetFirstPlayerController();
				APawn* StepPawn = StepPC ? StepPC->GetPawn() : nullptr;
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[STEP] [%s] role=%s step=%s pawn=%s"),
					*AFLCreator_NetTag(W.Get()), *RoleStr, *StepStr,
					StepPawn ? *StepPawn->GetName() : TEXT("<null>"));
				AFLCreator_RunArgs(W.Get(), Args);
			}), Delay, false);
		};

		// SERVER-SIDE KILL. The client-side cheat was a MEASURED no-op: SuicidePawn ran, logged success, and
		// nothing died -- because health is SERVER-AUTHORITATIVE and DamageSelfDestruct on a CLIENT ASC applies
		// nothing that survives. run_under_one_process (required anyway, since the harness discovers roles by
		// walking GetWorldContexts of THIS process) puts the dedicated-server world in reach, so drive the death
		// there directly. PlayerId is the join key: it is server-assigned and replicated, so it is the only
		// stable handle that means the same player in both worlds -- world/PlayerState/pawn names all collide.
		auto FireServerKill = [W, Role](float Delay, const TCHAR* Step)
		{
			if (!W.IsValid()) { return; }
			FTimerHandle T;
			const FString StepStr(Step); const FString RoleStr(Role);
			W->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([W, StepStr, RoleStr]()
			{
				if (!W.IsValid()) { return; }
				int32 MyId = -1;
				if (APlayerController* MyPC = W->GetFirstPlayerController())
				{
					if (APlayerState* MyPS = MyPC->PlayerState) { MyId = MyPS->GetPlayerId(); }
				}
				UWorld* SrvWorld = nullptr;
				if (GEngine)
				{
					for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
					{
						UWorld* CW = Ctx.World();
						if (CW && CW->IsGameWorld() && CW->GetNetMode() == NM_DedicatedServer) { SrvWorld = CW; break; }
					}
				}
				APawn* KilledPawn = nullptr;
				bool bFoundPC = false;
				if (SrvWorld && MyId >= 0)
				{
					for (FConstPlayerControllerIterator It = SrvWorld->GetPlayerControllerIterator(); It; ++It)
					{
						APlayerController* SrvPC = It->Get();
						APlayerState* SrvPS = SrvPC ? SrvPC->PlayerState : nullptr;
						if (!SrvPS || SrvPS->GetPlayerId() != MyId) { continue; }
						bFoundPC = true;
						if (APawn* SrvPawn = SrvPC->GetPawn())
						{
							// Same canonical path the cheat uses -- the SetByCaller damage GE execution, so
							// OnOutOfHealth actually fires (a base-value override would not).
							if (ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(SrvPawn))
							{
								KilledPawn = SrvPawn;
								HC->DamageSelfDestruct();
							}
						}
						break;
					}
				}
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[STEP] [%s] role=%s step=%s myPid=%d srvWorld=%s srvPC=%s killedPawn=%s"),
					*AFLCreator_NetTag(W.Get()), *RoleStr, *StepStr, MyId,
					SrvWorld ? TEXT("FOUND") : TEXT("MISSING"), bFoundPC ? TEXT("FOUND") : TEXT("MISSING"),
					KilledPawn ? *KilledPawn->GetName() : TEXT("<none>"));
			}), Delay, false);
		};

		// CONSOLE-EXEC ARM. Fire() routes ColorProbe ARGS; this runs an arbitrary console command on THIS
		// window own PC. Needed because a game world is required for catalog-backed commands, and bridge calls
		// are forbidden while PIE runs -- so anything needing a live world has to be fired from in here.
		auto FireCmd = [W, Role](float Delay, const TCHAR* Cmd, const TCHAR* Step)
		{
			if (!W.IsValid()) { return; }
			FTimerHandle T;
			const FString StepStr(Step); const FString RoleStr(Role); const FString CmdStr(Cmd);
			W->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([W, CmdStr, StepStr, RoleStr]()
			{
				if (!W.IsValid()) { return; }
				APlayerController* StepPC = W->GetFirstPlayerController();
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[STEP] [%s] role=%s step=%s cmd=%s"),
					*AFLCreator_NetTag(W.Get()), *RoleStr, *StepStr, *CmdStr);
				if (StepPC) { StepPC->ConsoleCommand(CmdStr, /*bWriteToLog=*/true); }
			}), Delay, false);
		};

		// CC-X15 step 4: assert the facemask command can reach EVERY catalog row. Role A only, once, early --
		// it is a pure read and must not perturb the colour timeline that follows.
		if (RoleIndex == 0) { FireCmd(2.0f, TEXT("afl.Cosmetic.SetFacemask verify"), TEXT("0-facemask-verify")); }
		if (RoleIndex == 0) { FireCmd(3.0f, TEXT("afl.Catalog.TypeLint"), TEXT("0-type-lint")); }
		// CC-3 build proof. Sequenced BEFORE the colour timeline so a build activation cannot be
		// confused with a direct colour set: the two write the same field by different routes.
		// RELAUNCH PROOF: pull from the remote WITHOUT seeding. If builds appear, they came from the
		// backend row written by a PREVIOUS process -- the definition of surviving a relaunch. Gated on
		// afl.Creator.PullOnly so the normal harness still seeds.
		static const auto* CVarPullOnly = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Creator.PullOnly"));
		const bool bPullOnly = CVarPullOnly && CVarPullOnly->GetInt() != 0;
		if (RoleIndex == 0 && bPullOnly) { FireCmd(4.0f, TEXT("afl.Creator.BuildProbe pull"), TEXT("R1-pull")); }
		if (RoleIndex == 0 && !bPullOnly) { FireCmd(4.0f,  TEXT("afl.Creator.BuildProbe seed"),  TEXT("b1-seed")); }
		if (RoleIndex == 0) { FireCmd(6.0f,  TEXT("afl.Creator.BuildProbe use 0"), TEXT("b2-use0")); }
		FireCmd(7.5f,  TEXT("afl.Creator.BuildProbe"), TEXT("b3-read-after-0"));
		if (RoleIndex == 0) { FireCmd(9.0f,  TEXT("afl.Creator.BuildProbe use 1"), TEXT("b4-use1")); }
		FireCmd(10.5f, TEXT("afl.Creator.BuildProbe"), TEXT("b5-read-after-1"));
		// CC-4.2 LAPSE. Cap 1 with 2 builds saved: build 1 must go read-only while build 0 stays
		// editable, the ACTIVE build must keep rendering exactly what it rendered, and the build
		// COUNT must not move. Then restore cap 2 and the lock must clear -- a rule that only ever
		// locks would pass the first half and strand every player who resubscribed.
		if (RoleIndex == 0) { FireCmd(12.5f, TEXT("afl.Creator.BuildProbe lapse 1 0"), TEXT("L1-lapse-cap1")); }
		FireCmd(13.5f, TEXT("afl.Creator.BuildProbe"), TEXT("L2-read-lapsed"));
		if (RoleIndex == 0) { FireCmd(15.0f, TEXT("afl.Creator.BuildProbe lapse 2 1"), TEXT("L3-restore-cap2")); }
		FireCmd(16.0f, TEXT("afl.Creator.BuildProbe"), TEXT("L4-read-restored"));
		// AFTER the 20s kill: does BuildSet survive the PlayerState swap (CopyProperties)? Untested
		// until now -- the earlier run had no post-respawn build read, so an emptied set would have
		// gone unnoticed while the resolved Selection kept rendering.
		FireCmd(30.0f, TEXT("afl.Creator.BuildProbe"), TEXT("b6-read-post-respawn"));

		TArray<FString> Read;  Read.Add(TEXT("read"));
		TArray<FString> Clear; Clear.Add(TEXT("clear"));
		// COLOUR CHOICE -- must be a REAL delta and must be UNIQUE TO A. Two defects made the last run
		// inconclusive and both are fixed here:
		//  (a) NO DELTA. The old hardcoded 0.2/0.2/0.22 clamps to (0.2025,0.2025,0.4500) -- EXACTLY the value
		//      persistence had already stored (it was that same clamp's output from an earlier run). So the
		//      "set" moved only bUseCreatorColors; the colour itself never changed. A fixed literal cannot fix
		//      this, because whatever we pick becomes NEXT run's inherited value and the trap reappears. So
		//      CHOOSE AGAINST WHAT WAS INHERITED: two well-separated candidates, take the farther one. Delta by
		//      construction, every run, regardless of what persistence restored.
		//  (b) NOT DISCRIMINABLE. Both players load the SAME persisted selection, so A's colour appearing in
		//      B's log proved nothing -- B's own pawn already rendered it. Only A sets, and it sets to a colour
		//      far from the shared inherited one, so a FINAL= carrying it in B's log can ONLY be A's pawn.
		//      That is what makes the REMOTE leg readable at all.
		// Both candidates are inside the neon gamut (S>=0.55, V in [0.45,1]) so ClampToNeon passes them through
		// and the value we assert on is the value we asked for.
		const FLinearColor CandP(0.90f, 0.05f, 0.60f);   // magenta
		const FLinearColor CandQ(0.05f, 0.90f, 0.80f);   // cyan
		FLinearColor Inherited(ForceInit);
		if (const UAFLCosmeticLoadoutComponent* L0 = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr)
		{
			Inherited = L0->GetSelection().CreatorBodyColor;
		}
		auto Dist2 = [](const FLinearColor& X, const FLinearColor& Y)
		{
			return FMath::Square(X.R - Y.R) + FMath::Square(X.G - Y.G) + FMath::Square(X.B - Y.B);
		};
		const FLinearColor Chosen = (Dist2(CandP, Inherited) >= Dist2(CandQ, Inherited)) ? CandP : CandQ;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[PICK] [%s] role=%s inherited=(%.4f,%.4f,%.4f) chosen=(%.4f,%.4f,%.4f) d2=%.4f"),
			*AFLCreator_NetTag(World), Role, Inherited.R, Inherited.G, Inherited.B,
			Chosen.R, Chosen.G, Chosen.B, Dist2(Chosen, Inherited));
		TArray<FString> Set;   Set.Add(TEXT("set"));
		Set.Add(FString::SanitizeFloat(Chosen.R));
		Set.Add(FString::SanitizeFloat(Chosen.G));
		Set.Add(FString::SanitizeFloat(Chosen.B));

		Fire(1.0f,  Read,  TEXT("1-inherited"));
		if (RoleIndex == 0) { Fire(4.0f, Clear, TEXT("2-clear-delta1")); }
		Fire(8.0f,  Read,  TEXT("3-after-clear"));
		if (RoleIndex == 0) { Fire(12.0f, Set,  TEXT("4-set-delta2")); }
		Fire(16.0f, Read,  TEXT("5-after-set"));
		// STEP 6 -- RESPAWN DURABILITY via a DETERMINISTIC death. This previously waited 75s for a bot to kill
		// A and reported NOT EXERCISED when none did: a test whose null result costs a whole session to
		// discover. SuicidePawn drives DamageSelfDestruct -> the genuine Lyra OnOutOfHealth flow (NOT Destroy),
		// so respawn/re-possession/OnPossessedPawnChanged fire exactly as in a real death -- deterministic
		// WITHOUT weakening what is exercised. Only A dies; B stays alive as the remote observer.
		if (RoleIndex == 0) { FireServerKill(20.0f, TEXT("6a-kill-A-serverside")); }
		// BOTH roles read at 28s (8s for death->respawn->re-possess->part re-spawn).
		//   On A: pawn= must DIFFER from step 5 -- that difference IS the respawn. Unchanged -> NOT EXERCISED.
		//   On B: this read reports B's OWN selection (AFLCreator_ReadCommitted resolves the LOCAL PlayerState,
		//   it cannot read a remote pawn). The REMOTE leg is therefore proven from the per-key FINAL= emits in
		//   B's log instead: A's respawn spawns NEW part actors on B too, whose PATH 1 BeginPlay read must
		//   re-apply the overlay. Attributable now that the SkinDiag prefix carries pid (= the OBSERVING
		//   client), with the fresh pawn appearing in B's world right after 20s being A's -- B does not die.
		Fire(28.0f, Read,  TEXT("6b-post-respawn"));
	}

	static TArray<TWeakObjectPtr<UWorld>> GAFLCreatorFiredWorlds;

	static FTSTicker::FDelegateHandle GAFLCreatorAutoTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([](float) -> bool
		{
			if (GAFLCreatorAutoProbe == 0 || !GEngine) { return true; }
			// STALE-ENTRY PURGE. GAFLCreatorFiredWorlds is a file-static that OUTLIVES a PIE session, and
			// RoleIndex is just its Num(). Without this, the SECOND PIE run in one editor process starts at
			// RoleIndex 2 -- so no world is RoleIndex 0 and EVERY role-A-only arm (clear, set, kill, the
			// verify/lint execs) is skipped in silence, with no error and a timeline that looks half-run.
			// Measured: a second run emitted idx=2 and idx=3, both B-OBSERVER. Worlds from an ended session
			// go stale, so dropping invalid entries re-bases the next session at 0.
			GAFLCreatorFiredWorlds.RemoveAll([](const TWeakObjectPtr<UWorld>& P) { return !P.IsValid(); });
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				UWorld* W = Ctx.World();
				if (!W || !W->IsGameWorld() || W->GetNetMode() != NM_Client) { continue; }
				bool bAlready = false;
				for (const TWeakObjectPtr<UWorld>& Seen : GAFLCreatorFiredWorlds) { if (Seen.Get() == W) { bAlready = true; break; } }
				if (bAlready) { continue; }
				APlayerController* PC = W->GetFirstPlayerController();
				APlayerState* PS = PC ? PC->PlayerState : nullptr;
				if (PS && PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>())
				{
					const int32 RoleIndex = GAFLCreatorFiredWorlds.Num();
					GAFLCreatorFiredWorlds.Add(W);
					AFLCreator_AutoSequence(W, RoleIndex);
				}
			}
			return true;
		}), 1.0f);
#endif // !UE_BUILD_SHIPPING

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCreatorColorProbeCmd(
		TEXT("afl.Creator.ColorProbe"),
		TEXT("CC-2.1-P/I2: drive the creator colour overlay through the REAL funnel (ServerSetCosmeticSelection -> server ClampToNeon -> commit -> replicate), then read the CLIENT-side MID params back. Modes: 'read' = readback only (R2/R4); 'clear' = bUseCreatorColors->0 through the funnel (guarantees the NEXT set is a delta, and exercises the turn-off path); 'set <bR> <bG> <bB> [eR eG eB gR gG gB]' = colours from ARGS. Every mode emits R0 (what the run INHERITED) so a no-delta run is self-evident. Pick a body colour outside the neon gamut (low S/V) to measure the clamp."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCreatorColorProbe));

	void HandleAFLCosmeticSetParam(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 4)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetParam — usage: afl.Cosmetic.SetParam <ParamName> <R> <G> <B> (0..1 each). e.g. afl.Cosmetic.SetParam TeamColor 1 0 0"));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetParam — no game world (run inside PIE)."));
			return;
		}
		const FName ParamName(*Args[0].TrimStartAndEnd());
		const FLinearColor Value(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]), 1.0f);

		TArray<AAFLCharacterPartActor*> Parts;
		GatherPlayerPartActors(World, Parts);
		if (Parts.Num() == 0)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetParam — no AAFLCharacterPartActor on the local player's pawn (robot body not spawned yet?)."));
			return;
		}
		int32 TotalSlots = 0;
		for (AAFLCharacterPartActor* Part : Parts)
		{
			TotalSlots += Part->DebugSetMIDVectorParam(ParamName, Value);
		}
		Ar.Logf(TEXT("afl.Cosmetic.SetParam — set %s = (%.2f, %.2f, %.2f) on %d part(s), %d MID slot(s). Watch which region changed."),
			*ParamName.ToString(), Value.R, Value.G, Value.B, Parts.Num(), TotalSlots);
	}

	void HandleAFLCosmeticSetParamScalar(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 2)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetParamScalar — usage: afl.Cosmetic.SetParamScalar <ParamName> <Value>. e.g. afl.Cosmetic.SetParamScalar EdgeGlowMagnitude 1"));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.SetParamScalar — no game world (run inside PIE)."));
			return;
		}
		const FName ParamName(*Args[0].TrimStartAndEnd());
		const float Value = FCString::Atof(*Args[1]);

		TArray<AAFLCharacterPartActor*> Parts;
		GatherPlayerPartActors(World, Parts);
		if (Parts.Num() == 0)
		{
			Ar.Log(TEXT("afl.Cosmetic.SetParamScalar — no AAFLCharacterPartActor on the local player's pawn (robot body not spawned yet?)."));
			return;
		}
		int32 TotalSlots = 0;
		for (AAFLCharacterPartActor* Part : Parts)
		{
			TotalSlots += Part->DebugSetMIDScalarParam(ParamName, Value);
		}
		Ar.Logf(TEXT("afl.Cosmetic.SetParamScalar — set %s = %.3f on %d part(s), %d MID slot(s). Watch which region changed."),
			*ParamName.ToString(), Value, Parts.Num(), TotalSlots);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetParamCmd(
		TEXT("afl.Cosmetic.SetParam"),
		TEXT("PANEL WATCH: poke a VECTOR material param on the player robot's live MIDs to see which region it paints. Usage: afl.Cosmetic.SetParam <ParamName> <R> <G> <B>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetParam));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetParamScalarCmd(
		TEXT("afl.Cosmetic.SetParamScalar"),
		TEXT("PANEL WATCH: poke a SCALAR material param on the player robot's live MIDs. Usage: afl.Cosmetic.SetParamScalar <ParamName> <Value> (e.g. EdgeGlowMagnitude 1)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticSetParamScalar));

	// ─── S-ECON-CAT 4b resolution probe: afl.Cosmetic.Resolve <id> ───────────────
	//
	// A diagnostic that resolves ANY catalog id through the subsystem and logs the result -- proves
	// catalog resolution end-to-end for the non-skin types (helmet, the EMP ability cosmetic) the same
	// way resolveVia=catalog proves it for the edge. Read-only: it does not apply or grant anything, just
	// confirms the catalog returns the right asset of the right type for an id. (The actual helmet-apply /
	// EMP-throw are watched separately; this isolates the CATALOG-RESOLUTION half for any id.)
	void HandleAFLCosmeticResolve(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			Ar.Log(TEXT("afl.Cosmetic.Resolve — usage: afl.Cosmetic.Resolve <AFL.Helmet.Visor01 | AFL.Ability.EMP | AFL.Edge.NeonGreen | ...>"));
			return;
		}
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.Resolve — no game world (run inside PIE)."));
			return;
		}
		UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!Catalog || !Catalog->IsReady())
		{
			Ar.Log(TEXT("afl.Cosmetic.Resolve — catalog subsystem not ready (no DA_AFL_CosmeticCatalog loaded)."));
			return;
		}

		const FName Id(*Args[0].TrimStartAndEnd());
		const FAFLCatalogEntry* Entry = Catalog->FindEntry(Id);
		UPrimaryDataAsset* Asset = Catalog->ResolveAsset(Id);

		if (!Entry)
		{
			Ar.Logf(TEXT("afl.Cosmetic.Resolve — id '%s' NOT in catalog (resolveVia=miss)."), *Id.ToString());
			return;
		}
		Ar.Logf(TEXT("afl.Cosmetic.Resolve — id=%s type=%d resolveVia=catalog asset=%s (volts=%d watts=%d acq=%d)"),
			*Id.ToString(), (int32)Entry->Type,
			Asset ? *Asset->GetName() : TEXT("<unset/failed-load>"),
			Entry->PriceVolts, Entry->PriceWatts, (int32)Entry->Acquisition);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticResolveCmd(
		TEXT("afl.Cosmetic.Resolve"),
		TEXT("S-ECON-CAT: resolve any catalog id through the subsystem + log the result (proves resolveVia=catalog for any type incl. helmet/EMP). Usage: afl.Cosmetic.Resolve <CosmeticId>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticResolve));

	// ─── S-ECON-CAT 4b EMP activate (1a): afl.Cosmetic.ActivateEMP ───────────────
	//
	// The ability-grant-path proof, end to end through the catalog: resolve AFL.Ability.EMP -> the
	// UAFLAbilityCosmeticAsset -> its AbilityClass -> find that ability's GRANTED spec on the player ASC
	// (granted via AbilitySet_AFL_EMP on HeroData_BagMan) -> TryActivateAbility (bAllowRemoteActivation so
	// the client->server activation is genuine). Sidesteps input-binding (1a ruling: binding to
	// InputTag.Ability.Grenade would conflict with the grenade ShooterHero already grants; real EMP input
	// is later polish). Watches the EMP throwing/functioning with the inherited grenade behavior +
	// resolveVia=catalog. Read-the-catalog-then-act: the cheat does not hardcode the ability class.
	void HandleAFLCosmeticActivateEMP(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Cosmetic.ActivateEMP — no game world (run inside PIE)."));
			return;
		}

		UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!Catalog || !Catalog->IsReady())
		{
			Ar.Log(TEXT("afl.Cosmetic.ActivateEMP — catalog not ready."));
			return;
		}

		// Resolve AFL.Ability.EMP -> UAFLAbilityCosmeticAsset -> the ability CLASS (catalog is the source).
		const FName EmpId(TEXT("AFL.Ability.EMP"));
		UAFLAbilityCosmeticAsset* AbilityCosmetic = Cast<UAFLAbilityCosmeticAsset>(Catalog->ResolveAsset(EmpId));
		if (!AbilityCosmetic)
		{
			Ar.Logf(TEXT("afl.Cosmetic.ActivateEMP — %s did not resolve to a UAFLAbilityCosmeticAsset (resolveVia=miss)."),
				*EmpId.ToString());
			return;
		}
		UClass* AbilityClass = AbilityCosmetic->AbilityClass.LoadSynchronous();
		if (!AbilityClass)
		{
			Ar.Log(TEXT("afl.Cosmetic.ActivateEMP — UAFLAbilityCosmeticAsset.AbilityClass unset/failed to load."));
			return;
		}

		// Find the GRANTED spec for that ability on the local player's ASC and activate it.
		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAbilitySystemComponent* ASC = PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr;
		if (!ASC)
		{
			Ar.Log(TEXT("afl.Cosmetic.ActivateEMP — no player ASC."));
			return;
		}

		FGameplayAbilitySpec* Found = nullptr;
		for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass)
			{
				Found = &Spec;
				break;
			}
		}
		if (!Found)
		{
			Ar.Logf(TEXT("afl.Cosmetic.ActivateEMP — resolveVia=catalog ability=%s but NOT granted on the player ")
				TEXT("(add AbilitySet_AFL_EMP to HeroData_BagMan)."), *AbilityClass->GetName());
			return;
		}

		const bool bActivated = ASC->TryActivateAbility(Found->Handle, /*bAllowRemoteActivation=*/true);
		Ar.Logf(TEXT("afl.Cosmetic.ActivateEMP — resolveVia=catalog ability=%s granted=yes activated=%s"),
			*AbilityClass->GetName(), bActivated ? TEXT("yes") : TEXT("no(cooldown/cost?)"));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticActivateEMPCmd(
		TEXT("afl.Cosmetic.ActivateEMP"),
		TEXT("S-ECON-CAT 4b: resolve AFL.Ability.EMP via catalog -> ability class -> activate the granted spec (proves the ability-grant path + resolveVia=catalog). Throws the EMP (inherited grenade behavior)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticActivateEMP));

	// ─── S-ECON-WALLET: afl.Wallet.* (balance / gate / earn-spend, server-authoritative) ─────────────
	//
	// Drive the genuine UAFLWalletComponent paths (PURE callers, like SetEdge): Get reads the replicated
	// balance/ownership; Set/Grant are dev seeds (authority); Earn/Buy call the Server RPCs (client->server
	// hop, server validates). Enable `afl.WalletDiag 1` to watch the per-layer logs (balance/gate/earn-spend).
	// The wallet is on the local player's PlayerState (same as the loadout).
	UAFLWalletComponent* GetPlayerWallet(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		return PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	}

	void HandleAFLWalletGet(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Wallet.Get - run inside PIE.")); return; }
		UAFLWalletComponent* W = GetPlayerWallet(World);
		if (!W) { Ar.Log(TEXT("afl.Wallet.Get - no UAFLWalletComponent on the local player's PlayerState.")); return; }
		Ar.Logf(TEXT("afl.Wallet.Get - Volts=%d Watts=%d  (run afl.WalletDiag 1 for per-layer logs)"), W->GetVolts(), W->GetWatts());
	}

	void HandleAFLWalletSet(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 2) { Ar.Log(TEXT("afl.Wallet.Set - usage: afl.Wallet.Set <Volts> <Watts> (dev seed, authority)")); return; }
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Wallet.Set - run inside PIE.")); return; }
		UAFLWalletComponent* W = GetPlayerWallet(World);
		if (!W) { Ar.Log(TEXT("afl.Wallet.Set - no wallet component.")); return; }
		W->DebugSetBalance(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		Ar.Logf(TEXT("afl.Wallet.Set - requested Volts=%s Watts=%s (authority-only; watch replication on the client)."), *Args[0], *Args[1]);
	}

	void HandleAFLWalletEarn(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1) { Ar.Log(TEXT("afl.Wallet.Earn - usage: afl.Wallet.Earn <Watts> [Volts] (server-validated earn)")); return; }
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Wallet.Earn - run inside PIE.")); return; }
		UAFLWalletComponent* W = GetPlayerWallet(World);
		if (!W) { Ar.Log(TEXT("afl.Wallet.Earn - no wallet component.")); return; }
		const int32 EarnW = FCString::Atoi(*Args[0]);
		W->ServerEarnWatts(EarnW); // client->server hop; server validates + commits.
		if (Args.Num() >= 2) { W->ServerEarnVolts(FCString::Atoi(*Args[1])); }
		Ar.Logf(TEXT("afl.Wallet.Earn - client issued ServerEarnWatts(%d)%s. Server validates; balance replicates down."),
			EarnW, Args.Num() >= 2 ? *FString::Printf(TEXT(" + ServerEarnVolts(%s)"), *Args[1]) : TEXT(""));
	}

	void HandleAFLWalletBuy(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1) { Ar.Log(TEXT("afl.Wallet.Buy - usage: afl.Wallet.Buy <CosmeticId> [volts|watts|auto]  e.g. afl.Wallet.Buy AFL.Edge.NeonBlue volts")); return; }
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Wallet.Buy - run inside PIE.")); return; }
		UAFLWalletComponent* W = GetPlayerWallet(World);
		if (!W) { Ar.Log(TEXT("afl.Wallet.Buy - no wallet component.")); return; }
		const FName Id(*Args[0].TrimStartAndEnd());

		// Optional 2nd arg = the currency the player chooses (SPARK is pay-either). Default Auto.
		EAFLPayCurrency Pay = EAFLPayCurrency::Auto;
		if (Args.Num() >= 2)
		{
			const FString C = Args[1].TrimStartAndEnd();
			if      (C.Equals(TEXT("volts"), ESearchCase::IgnoreCase)) Pay = EAFLPayCurrency::Volts;
			else if (C.Equals(TEXT("watts"), ESearchCase::IgnoreCase)) Pay = EAFLPayCurrency::Watts;
			else if (C.Equals(TEXT("auto"),  ESearchCase::IgnoreCase)) Pay = EAFLPayCurrency::Auto;
			else { Ar.Logf(TEXT("afl.Wallet.Buy - unknown currency '%s' (use volts|watts|auto); defaulting Auto."), *C); }
		}
		const TCHAR* PayStr = (Pay == EAFLPayCurrency::Volts) ? TEXT("volts") : (Pay == EAFLPayCurrency::Watts) ? TEXT("watts") : TEXT("auto");

		W->ServerPurchaseCosmetic(Id, Pay); // server reads catalog price, validates balance, deducts + grants.
		Ar.Logf(TEXT("afl.Wallet.Buy - client issued ServerPurchaseCosmetic(%s, pay=%s). Server: price from catalog -> deduct -> grant. Watch afl.WalletDiag 1."), *Id.ToString(), PayStr);
	}

	void HandleAFLWalletGrant(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1) { Ar.Log(TEXT("afl.Wallet.Grant - usage: afl.Wallet.Grant <CosmeticId> (dev grant, authority - test the gate without spending)")); return; }
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Wallet.Grant - run inside PIE.")); return; }
		UAFLWalletComponent* W = GetPlayerWallet(World);
		if (!W) { Ar.Log(TEXT("afl.Wallet.Grant - no wallet component.")); return; }
		const FName Id(*Args[0].TrimStartAndEnd());
		W->DebugGrantOwnership(Id);
		Ar.Logf(TEXT("afl.Wallet.Grant - requested ownership of %s (authority). The entitlement gate now resolves it owned."), *Id.ToString());
	}

	// A1.3a PROOF-2 regression harness (extends the DebugVerifyA12 pattern). Asserts the shipping earn-guard
	// did NOT break either legit earn path -- run on the LISTEN-HOST window (both mutate via the authority
	// commit funnel synchronously). The forge-closure itself is the #if UE_BUILD_SHIPPING early-return
	// (cook-confirmable by shipping-inertness, not a PIE runtime check).
	void HandleAFLWalletVerifyA13a(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Wallet.VerifyA13a - run inside PIE (listen-host window).")); return; }
		UAFLWalletComponent* W = GetPlayerWallet(World);
		if (!W) { Ar.Log(TEXT("afl.Wallet.VerifyA13a - no wallet component.")); return; }
		if (!W->GetOwner() || !W->GetOwner()->HasAuthority())
		{
			Ar.Log(TEXT("afl.Wallet.VerifyA13a - not authority; run on the listen-host window."));
			UE_LOG(LogTemp, Warning, TEXT("AFL_A13A FAIL: not authority (run on host)."));
			return;
		}

		// PROOF-2a: the AUTHORITY earn sink -- EarnWattsAuthority, the EXACT method extraction uses
		// (AFLAG_Extract.cpp:135). Untouched by A1.3a; must still credit.
		const int32 W0 = W->GetWatts();
		W->EarnWattsAuthority(100, TEXT("VerifyA13a"));
		const int32 W1 = W->GetWatts();
		const bool bAuthorityOk = (W1 == W0 + 100);
		UE_LOG(LogTemp, Display, TEXT("AFL_A13A[authority-earn] %s (watts %d->%d, +100 via EarnWattsAuthority = extraction's sink)"),
			bAuthorityOk ? TEXT("PASS") : TEXT("FAIL"), W0, W1);

		// PROOF-2b: the DEV earn tool -- ServerEarnWatts/Volts, now #if UE_BUILD_SHIPPING body-guarded so INERT
		// in shipping but LIVE here in the editor/dev build; must still credit (proves the guard didn't break dev).
		const int32 V0 = W->GetVolts();
		W->ServerEarnWatts(50);
		W->ServerEarnVolts(25);
		const int32 W2 = W->GetWatts();
		const int32 V1 = W->GetVolts();
		const bool bToolOk = (W2 == W1 + 50) && (V1 == V0 + 25);
		UE_LOG(LogTemp, Display, TEXT("AFL_A13A[dev-earn-tool] %s (watts %d->%d +50, volts %d->%d +25 via ServerEarnWatts/Volts dev RPCs)"),
			bToolOk ? TEXT("PASS") : TEXT("FAIL"), W1, W2, V0, V1);

		const bool bAll = bAuthorityOk && bToolOk;
		UE_LOG(LogTemp, Display, TEXT("AFL_A13A COMPLETE authority=%d devTool=%d -- %s. (Forge-closure = shipping body-guard, cook-confirmable; not a PIE check.)"),
			bAuthorityOk ? 1 : 0, bToolOk ? 1 : 0, bAll ? TEXT("PASS") : TEXT("FAIL"));
		Ar.Logf(TEXT("afl.Wallet.VerifyA13a - authority=%d devTool=%d. WATCH the on-screen AFL_A13A PASS/FAIL; AIK reads the log after PIE closes."),
			bAuthorityOk ? 1 : 0, bToolOk ? 1 : 0);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 12.f, bAll ? FColor::Green : FColor::Red,
				FString::Printf(TEXT("[A13a] earn regression %s (authority=%d devTool=%d)"), bAll ? TEXT("PASS") : TEXT("FAIL"), bAuthorityOk ? 1 : 0, bToolOk ? 1 : 0));
		}
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLWalletGetCmd(TEXT("afl.Wallet.Get"),
		TEXT("S-ECON-WALLET: print the player's replicated Volts/Watts balance."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLWalletGet));
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLWalletSetCmd(TEXT("afl.Wallet.Set"),
		TEXT("S-ECON-WALLET (a) balance: dev-seed the balance (authority). Usage: afl.Wallet.Set <Volts> <Watts>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLWalletSet));
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLWalletEarnCmd(TEXT("afl.Wallet.Earn"),
		TEXT("S-ECON-WALLET (c) earn: server-validated earn. Usage: afl.Wallet.Earn <Watts> [Volts]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLWalletEarn));
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLWalletBuyCmd(TEXT("afl.Wallet.Buy"),
		TEXT("S-ECON-WALLET (c) spend + (b) grant: buy a catalog cosmetic (server reads price, deducts, grants ownership). Usage: afl.Wallet.Buy <CosmeticId>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLWalletBuy));
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLWalletGrantCmd(TEXT("afl.Wallet.Grant"),
		TEXT("S-ECON-WALLET (b) gate: dev-grant ownership without spending (test the entitlement gate). Usage: afl.Wallet.Grant <CosmeticId>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLWalletGrant));
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLWalletVerifyA13aCmd(TEXT("afl.Wallet.VerifyA13a"),
		TEXT("A1.3a regression: assert both legit earn paths survive the shipping earn-guard -- (a) EarnWattsAuthority (extraction's authority sink) credits +100 Watts, (b) the dev ServerEarnWatts/Volts RPCs credit +50/+25. AFL_A13A PASS = both. Forge-closure itself = the #if UE_BUILD_SHIPPING early-return (cook-confirmable). Listen-host window."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLWalletVerifyA13a));

	// ─── A1.3 step-3 earn-hook proof: afl.Wallet.VerifyA13s3 ───────────────────────
	// Drives the REAL earn funnel (EarnWattsAuthority) deterministically -- no in-game extraction needed. Same
	// method + reason tag AFLAG_Extract.cpp:135 uses, so it hits the IDENTICAL path: CommitMutation (local +50)
	// THEN the A1.3 step-3 PlayFab-push hook (gate -> skip on non-dedicated PIE, or push on a true dedicated
	// server). Authority-only like the extraction commit -> run on a listen-host/standalone (local = authority).
	void HandleAFLWalletVerifyA13s3(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Wallet.VerifyA13s3 - run inside PIE (listen-host/standalone).")); return; }
		UAFLWalletComponent* W = GetPlayerWallet(World);
		if (!W) { Ar.Log(TEXT("afl.Wallet.VerifyA13s3 - no wallet component.")); return; }
		if (!W->GetOwner() || !W->GetOwner()->HasAuthority())
		{
			Ar.Log(TEXT("afl.Wallet.VerifyA13s3 - not authority; run on a listen-host/standalone (EarnWattsAuthority is authority-only, exactly like the extraction commit)."));
			return;
		}

		// Drive the REAL funnel with the real reason tag + a fixed test amount -- EXACTLY what a real extraction
		// does: Wallet->EarnWattsAuthority(Reward, TEXT("extraction")). No re-implementation, no gate bypass.
		const int32 W0 = W->GetWatts();
		UE_LOG(LogTemp, Display, TEXT("AFL_A13S3_CHEAT fired amount=50 reason=extraction"));
		W->EarnWattsAuthority(50, TEXT("extraction"));
		const int32 W1 = W->GetWatts();

		// Local credit must have banked (CommitMutation ran first; the step-3 hook runs AFTER and never affects it).
		const bool bCredited = (W1 == W0 + 50);
		UE_LOG(LogTemp, Display, TEXT("AFL_A13S3_CHEAT local credit %s (watts %d->%d, +50). The step-3 hook then fires -> expect AFL_A13S3 skip: not dedicated server in PIE (the grant is dedicated-server-only)."),
			bCredited ? TEXT("PASS") : TEXT("FAIL"), W0, W1);
		Ar.Logf(TEXT("afl.Wallet.VerifyA13s3 - EarnWattsAuthority(50) -> watts %d->%d (+50 %s). Read the log for AFL_A13S3 skip: not dedicated server (the anti-spoof gate)."),
			W0, W1, bCredited ? TEXT("credited") : TEXT("NO-CREDIT!!"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 12.f, bCredited ? FColor::Green : FColor::Red,
				FString::Printf(TEXT("[A13s3] real funnel -> local +50 %s (watts %d->%d); step-3 hook gate-skips on non-dedicated"), bCredited ? TEXT("PASS") : TEXT("FAIL"), W0, W1));
		}
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLWalletVerifyA13s3Cmd(TEXT("afl.Wallet.VerifyA13s3"),
		TEXT("A1.3 step-3 earn-hook proof: drive the REAL earn funnel EarnWattsAuthority(50,\"extraction\") -- the EXACT path a real extraction hits. Runs CommitMutation (local +50) THEN the step-3 PlayFab-push hook (gate skips on non-dedicated PIE, pushes on a true dedicated server). Watch: +50 credited + AFL_A13S3 skip: not dedicated server. Authority-only -- run on a listen-host/standalone."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLWalletVerifyA13s3));

	// ─── A1.1 backend-verify harness: afl.Online.VerifyA11 ───────────────────────
	// Automates the cross-device proof in ONE PIE (mirror of afl.Cosmetic.Cycle's dual-verify): WIPE local
	// cache -> PlayFab login -> load -> ASSERT the seeded truth + source=PlayFab. Because local is wiped
	// FIRST, a correct read can ONLY come from the account (PlayFab), not the machine -> "wiped-local +
	// still-loaded" == cross-device proven, no 2nd machine. Sibling verifies for A1.2/A1.3 follow the same
	// afl.Online.Verify<phase> shape (the standing backend-verify pattern, like afl.Cosmetic.Cycle for skins).
	void HandleAFLOnlineVerifyA11(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.VerifyA11 - run inside PIE.")); return; }
		UGameInstance* GI = World->GetGameInstance();
		UAFLEconomyPersistenceSubsystem* Persist = GI ? GI->GetSubsystem<UAFLEconomyPersistenceSubsystem>() : nullptr;
		if (!Persist) { Ar.Log(TEXT("afl.Online.VerifyA11 - no economy persistence subsystem.")); return; }

		// FIXTURE-ROBUST (2026-07-08): A11 proves the CROSS-DEVICE property (wiped local -> still loaded from the
		// account) + ownership of the seeded SKU + positive currency loaded -- NOT a brittle exact VO/WA. Currency
		// legitimately moves (e.g. the Phase-2 Visors canary reseeds VO 1234->8000->3000), so the STABLE grant is the
		// value assertion, not the balance. Mirrors VerifyPurchaseSeam's relative/structural (delta) fixture style.
		const FName ExpectSku(TEXT("AFL.Beam.CrimsonArc"));

		// STEP 1 -- WIPE LOCAL: remove any local cache so a correct read PROVES it came from the account.
		Persist->DebugWipeLocalCache();
		UE_LOG(LogTemp, Display, TEXT("AFL_TEST[A11] local-wiped"));
		Ar.Log(TEXT("AFL_TEST[A11] local-wiped -> login -> load -> assert. WATCH the on-screen PASS/FAIL; AIK reads the log after PIE closes."));
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, TEXT("[A11] local wiped -> loading from PlayFab...")); }

		// STEP 2/3/4 -- login -> load straight from PlayFab -> assert (async).
		Persist->DebugProbePlayFabLoad([ExpectSku](bool bLoginOk, const FString& PfId, int32 VO, int32 WA, const TArray<FName>& Owned, bool bFromPlayFab)
		{
			UE_LOG(LogTemp, Display, TEXT("AFL_TEST[A11] login %s PlayFabId=%s"),
				bLoginOk ? TEXT("OK") : TEXT("FAIL"), PfId.IsEmpty() ? TEXT("<none>") : *PfId);

			FString OwnedStr;
			for (const FName& Id : Owned) { OwnedStr += (OwnedStr.IsEmpty() ? TEXT("") : TEXT(",")); OwnedStr += Id.ToString(); }
			UE_LOG(LogTemp, Display, TEXT("AFL_TEST[A11] loaded VO=%d WA=%d owned=[%s] source=%s"),
				VO, WA, *OwnedStr, bFromPlayFab ? TEXT("PLAYFAB") : TEXT("CACHE_OR_MISS"));

			const bool bOwns = Owned.Contains(ExpectSku);
			const bool bPass = bLoginOk && bFromPlayFab && VO > 0 && WA > 0 && bOwns;

			if (bPass)
			{
				UE_LOG(LogTemp, Display, TEXT("AFL_TEST[A11] PASS source=PlayFab(not-local) VO=%d WA=%d owns=%s -- cross-device proven (local wiped, still loaded from the account)."),
					VO, WA, *ExpectSku.ToString());
				if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green, FString::Printf(TEXT("[A11] PASS  PlayFab VO=%d WA=%d + CrimsonArc  (local was wiped)"), VO, WA)); }
			}
			else
			{
				FString Reason;
				if (!bLoginOk)                             { Reason = TEXT("login-failed"); }
				else if (!bFromPlayFab)                    { Reason = TEXT("source-not-PlayFab (offline/HTTP-fail -> cache/miss)"); }
				else if (VO <= 0 || WA <= 0)               { Reason = FString::Printf(TEXT("non-positive currency loaded (VO=%d WA=%d) -- account seeded?"), VO, WA); }
				else if (!bOwns)                           { Reason = TEXT("missing AFL.Beam.CrimsonArc (seeded?)"); }
				else                                       { Reason = TEXT("unknown"); }
				UE_LOG(LogTemp, Warning, TEXT("AFL_TEST[A11] FAIL %s (login=%d source=%s VO=%d WA=%d owns=%d)"),
					*Reason, bLoginOk ? 1 : 0, bFromPlayFab ? TEXT("PLAYFAB") : TEXT("CACHE_OR_MISS"), VO, WA, bOwns ? 1 : 0);
				if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("[A11] FAIL: %s"), *Reason)); }
			}
		});
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLOnlineVerifyA11Cmd(TEXT("afl.Online.VerifyA11"),
		TEXT("A1.1 backend-verify harness: WIPE local cache -> PlayFab login -> load -> ASSERT cross-device (source=PlayFab not-local) + owns AFL.Beam.CrimsonArc + positive VO/WA loaded. FIXTURE-ROBUST: asserts the load-from-account property + the STABLE grant, NOT a brittle exact balance (currency legitimately moves). Wiped-local + still-loaded == cross-device proven in ONE PIE. Seed AFL_DEV_TEST_01 first."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLOnlineVerifyA11));

	// ─── A1.2 backend-verify harness: afl.Online.VerifyA12 ───────────────────────
	// Automates the anti-spoof PURCHASE proof (sibling of VerifyA11): fake a HIGH local balance -> the wallet
	// buys a stackable test token via PlayFab -> asserts (a) PlayFab deducted+granted SERVER-SIDE + mirror-
	// deducted locally, (b) a fake-price buy is REJECTED, (c) an over-balance buy is REJECTED (the faked local
	// balance is UNSPENDABLE = spend spoof closed). AFL_TEST[A12] PASS = all. Needs AFL.Test.Token + Premium
	// in the catalog (run setup:economy after the manifest add).
	void HandleAFLOnlineVerifyA12(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.VerifyA12 - run inside PIE.")); return; }
		UAFLWalletComponent* Wallet = GetPlayerWallet(World);
		if (!Wallet) { Ar.Log(TEXT("afl.Online.VerifyA12 - no wallet on the local PlayerState.")); return; }

		// Fake a HIGH local balance -> the spend-spoof step proves PlayFab ignores it (spends PlayFab-held only).
		Wallet->DebugSetBalance(9999999, 9999999);
		UE_LOG(LogTemp, Display, TEXT("AFL_TEST[A12] start -- faked local balance HIGH (9999999); buying via PlayFab..."));
		Ar.Log(TEXT("AFL_TEST[A12] start -> buy token via PlayFab -> assert deduct+grant + spoof-rejected + faked-local-unspendable. Watch the on-screen result; AIK reads the log after PIE."));
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, TEXT("[A12] local balance faked HIGH -> buying via PlayFab...")); }

		const FName TokenId(TEXT("AFL.Test.Token"));
		const FName PremiumId(TEXT("AFL.Test.Premium"));
		Wallet->DebugVerifyA12(TokenId, PremiumId, [](const FAFLPurchaseVerifyResult& R)
		{
			const bool bServerDeducted = (R.VoBefore >= 0 && R.VoAfter >= 0 && R.VoAfter == R.VoBefore - 10);
			UE_LOG(LogTemp, Display, TEXT("AFL_TEST[A12] login=%d serverVO %d->%d (deducted=%d) grantedOnPlayFab=%d mirrorDeducted=%d spoofRejected=%d spendSpoofRejected=%d"),
				R.bLoginOk ? 1 : 0, R.VoBefore, R.VoAfter, bServerDeducted ? 1 : 0,
				R.bLegitOwnedOnPlayFab ? 1 : 0, R.bMirrorDeducted ? 1 : 0, R.bSpoofRejected ? 1 : 0, R.bSpendSpoofRejected ? 1 : 0);

			const bool bPass = R.bLoginOk && bServerDeducted && R.bLegitOwnedOnPlayFab && R.bMirrorDeducted
				&& R.bSpoofRejected && R.bSpendSpoofRejected;
			if (bPass)
			{
				UE_LOG(LogTemp, Display, TEXT("AFL_TEST[A12] PASS -- PlayFab deducted+granted server-side; fake-price REJECTED; faked-local-balance UNSPENDABLE (spend spoof CLOSED)."));
				if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green, TEXT("[A12] PASS  server deduct+grant | fake-price rejected | faked-local unspendable")); }
			}
			else
			{
				FString Why;
				if (!R.bLoginOk) { Why = TEXT("login"); }
				else if (!bServerDeducted) { Why = FString::Printf(TEXT("server-deduct (VO %d->%d want -10; token seeded/enough?)"), R.VoBefore, R.VoAfter); }
				else if (!R.bLegitOwnedOnPlayFab) { Why = TEXT("token not granted on PlayFab"); }
				else if (!R.bMirrorDeducted) { Why = TEXT("local mirror-deduct"); }
				else if (!R.bSpoofRejected) { Why = TEXT("fake-price NOT rejected (!!)"); }
				else if (!R.bSpendSpoofRejected) { Why = TEXT("over-balance NOT rejected (!!)"); }
				else { Why = R.FailNote.IsEmpty() ? TEXT("unknown") : R.FailNote; }
				UE_LOG(LogTemp, Warning, TEXT("AFL_TEST[A12] FAIL: %s"), *Why);
				if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("[A12] FAIL: %s"), *Why)); }
			}
		});
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLOnlineVerifyA12Cmd(TEXT("afl.Online.VerifyA12"),
		TEXT("A1.2 backend-verify harness: fake a HIGH local balance, then buy a stackable test token via PlayFab -> assert (a) PlayFab deducted+granted server-side + mirror-deducted locally, (b) fake-price buy REJECTED, (c) over-balance buy REJECTED (faked local balance UNSPENDABLE = spend spoof closed). AFL_TEST[A12] PASS = all. Needs AFL.Test.Token + AFL.Test.Premium in the catalog (run setup:economy)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLOnlineVerifyA12));

	// ─── Phase-1 PRODUCTION-seam purchase proof: afl.Online.VerifyPurchaseSeam ────────────────────
	// Sibling of VerifyA12, but drives the REAL store entry ClientRequestPurchase -> PurchaseThroughBackend
	// (the Phase-1 RELOCATED transport) instead of the inline A12_TryBuy probe -- closing the gap that no test
	// exercised the production purchase path. Buys the transient-injected AFL.Test.Token (10 VO) through the
	// production entry, then over-buys Premium through the SAME entry to prove the spend wall still holds THROUGH
	// the seam. AFL_TEST[SEAM] PASS = seam fired + PlayFab deducted+granted + local mirror + spend-spoof rejected.
	void HandleAFLOnlineVerifyPurchaseSeam(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.VerifyPurchaseSeam - run inside PIE.")); return; }
		UAFLWalletComponent* Wallet = GetPlayerWallet(World);
		if (!Wallet) { Ar.Log(TEXT("afl.Online.VerifyPurchaseSeam - no wallet on the local PlayerState.")); return; }

		UE_LOG(LogTemp, Display, TEXT("AFL_TEST[SEAM] start -- buying AFL.Test.Token via the PRODUCTION entry (ClientRequestPurchase -> PurchaseThroughBackend)..."));
		Ar.Log(TEXT("AFL_TEST[SEAM] start -> ClientRequestPurchase(AFL.Test.Token, Volts) -> assert seam fired + PlayFab deduct+grant + mirror + spend-spoof rejected. Watch the on-screen result; AIK reads the log after PIE."));
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, TEXT("[SEAM] buying test token via the PRODUCTION purchase path...")); }

		Wallet->DebugVerifyPurchaseSeam([](const FAFLPurchaseVerifyResult& R)
		{
			const bool bServerDeducted = (R.VoBefore >= 0 && R.VoAfter >= 0 && R.VoAfter == R.VoBefore - 10);
			UE_LOG(LogTemp, Display, TEXT("AFL_TEST[SEAM] login=%d seamAccepted=%d serverVO %d->%d (deducted=%d) grantedOnPlayFab=%d mirrorDeducted=%d spendSpoofRejected=%d"),
				R.bLoginOk ? 1 : 0, R.bSeamAccepted ? 1 : 0, R.VoBefore, R.VoAfter, bServerDeducted ? 1 : 0,
				R.bLegitOwnedOnPlayFab ? 1 : 0, R.bMirrorDeducted ? 1 : 0, R.bSpendSpoofRejected ? 1 : 0);

			const bool bPass = R.bLoginOk && R.bSeamAccepted && bServerDeducted && R.bLegitOwnedOnPlayFab
				&& R.bMirrorDeducted && R.bSpendSpoofRejected;
			if (bPass)
			{
				UE_LOG(LogTemp, Display, TEXT("AFL_TEST[SEAM] PASS -- ClientRequestPurchase drove PurchaseThroughBackend (the Phase-1 relocated transport) to PlayFab: deducted+granted server-side; local mirror reflected; over-balance buy REJECTED through the SAME entry."));
				if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green, TEXT("[SEAM] PASS  production ClientRequestPurchase->PurchaseThroughBackend | deduct+grant | mirror | spend-spoof rejected")); }
			}
			else
			{
				FString Why;
				if (!R.bLoginOk) { Why = TEXT("login"); }
				else if (!R.bSeamAccepted) { Why = R.FailNote.IsEmpty() ? TEXT("seam NOT accepted (ClientRequestPurchase->PurchaseThroughBackend did not commit; not-in-catalog / PlayFab rejected)") : R.FailNote; }
				else if (!bServerDeducted) { Why = FString::Printf(TEXT("server-deduct (VO %d->%d want -10)"), R.VoBefore, R.VoAfter); }
				else if (!R.bLegitOwnedOnPlayFab) { Why = TEXT("token not granted on PlayFab (units did not increase)"); }
				else if (!R.bMirrorDeducted) { Why = TEXT("local mirror-deduct (ApplyPurchaseResult)"); }
				else if (!R.bSpendSpoofRejected) { Why = TEXT("over-balance NOT rejected through the seam (!!)"); }
				else { Why = R.FailNote.IsEmpty() ? TEXT("unknown") : R.FailNote; }
				UE_LOG(LogTemp, Warning, TEXT("AFL_TEST[SEAM] FAIL: %s"), *Why);
				if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("[SEAM] FAIL: %s"), *Why)); }
			}
		});
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLOnlineVerifyPurchaseSeamCmd(TEXT("afl.Online.VerifyPurchaseSeam"),
		TEXT("Phase-1 PRODUCTION-seam purchase proof: drive the REAL entry ClientRequestPurchase -> PurchaseThroughBackend (the relocated transport) to buy the transient-injected AFL.Test.Token (10 VO) via PlayFab, then over-buy Premium -> assert seam fired + PlayFab deduct+grant + local mirror + spend-spoof rejected THROUGH the seam. AFL_TEST[SEAM] PASS = all."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLOnlineVerifyPurchaseSeam));

	// ─── Phase-2 Visors canary: afl.Online.VerifyVisorCanary ──────────────────────
	// Sibling of VerifyPurchaseSeam, extended to the FULL part flow on a REAL seeded SKU: buy
	// AFL.Facemask.Flag.Japan (5000 VO) through the PRODUCTION entry ClientRequestPurchase -> the Phase-1 seam
	// -> PlayFab PurchaseItem, THEN equip it via the production ServerSetCosmeticSelection (the entitlement gate
	// now passes because the buy made it owned) -> the runtime path swaps the hero's slot-1 facemask material.
	// Proves buy -> entitled -> equip -> render on a catalog-seeded SKU end-to-end. The RENDER must be WATCHED
	// (Flag.Japan is visually distinct from the base default). DRIVES the existing chain ONLY -- ClientRequestPurchase,
	// the seam, ServerSetCosmeticSelection, and RefreshFacemaskForPawn are UNCHANGED. Dev-only (#if !UE_BUILD_SHIPPING).
	// AFL_TEST[VISOR] markers: purchased / granted / equipped / render-refreshed.
#if !UE_BUILD_SHIPPING
	void HandleAFLOnlineVerifyVisorCanary(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.VerifyVisorCanary - run inside PIE.")); return; }
		UAFLWalletComponent* Wallet = GetPlayerWallet(World);
		if (!Wallet) { Ar.Log(TEXT("afl.Online.VerifyVisorCanary - no wallet on the local PlayerState.")); return; }
		APlayerController* PC = World->GetFirstPlayerController();
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!Loadout) { Ar.Log(TEXT("afl.Online.VerifyVisorCanary - no UAFLCosmeticLoadoutComponent on the local player's PlayerState.")); return; }

		static const FName VisorId(TEXT("AFL.Facemask.Flag.Japan"));

		UE_LOG(LogTemp, Display, TEXT("AFL_TEST[VISOR] start -- buying %s (5000 VO) via the PRODUCTION entry ClientRequestPurchase -> Phase-1 seam -> PlayFab, then equip -> render-refresh."), *VisorId.ToString());
		Ar.Log(TEXT("AFL_TEST[VISOR] start -> ClientRequestPurchase(AFL.Facemask.Flag.Japan, Volts) -> equip via ServerSetCosmeticSelection -> WATCH the hero's facemask swap to the Japan flag. AIK/operator reads the log + WATCHES the render."));
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, TEXT("[VISOR] buying Flag.Japan via the PRODUCTION purchase path...")); }

		TWeakObjectPtr<UAFLWalletComponent> WeakWallet(Wallet);
		TWeakObjectPtr<UAFLCosmeticLoadoutComponent> WeakLoadout(Loadout);

		// Drive the REAL production chain: ClientRequestPurchase(callback) -> the seam -> PlayFab. On completion,
		// assert granted, then EQUIP through the production ServerSetCosmeticSelection (same as afl.Cosmetic.SetFacemask).
		Wallet->ClientRequestPurchase(VisorId, EAFLPayCurrency::Volts, [WeakWallet, WeakLoadout](bool bSuccess)
		{
			UAFLWalletComponent* W = WeakWallet.Get();
			UAFLCosmeticLoadoutComponent* L = WeakLoadout.Get();
			if (!W || !L) { UE_LOG(LogTemp, Warning, TEXT("AFL_TEST[VISOR] FAIL: wallet/loadout gone before purchase completion.")); return; }

			UE_LOG(LogTemp, Display, TEXT("AFL_TEST[VISOR] purchased ok=%d -- ClientRequestPurchase -> Phase-1 seam -> PlayFab PurchaseItem for %s"), bSuccess ? 1 : 0, *VisorId.ToString());
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("AFL_TEST[VISOR] FAIL: purchase rejected (funds/price/not-in-catalog). Ensure account VO>=5000 and AFL.Facemask.Flag.Japan seeded at VO 5000."));
				if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, TEXT("[VISOR] FAIL: purchase rejected")); }
				return;
			}

			// granted: the buy applied ownership on the local mirror (ApplyPurchaseResult) -> entitlement now owns it.
			const bool bGranted = W->IsEntitled(nullptr, VisorId);
			UE_LOG(LogTemp, Display, TEXT("AFL_TEST[VISOR] granted=%d -- OwnedCosmeticIds contains %s (entitlement gate will pass)"), bGranted ? 1 : 0, *VisorId.ToString());
			if (!bGranted)
			{
				UE_LOG(LogTemp, Warning, TEXT("AFL_TEST[VISOR] FAIL: purchased but not owned (ApplyPurchaseResult did not add the id)."));
				if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, TEXT("[VISOR] FAIL: granted=0")); }
				return;
			}

			// EQUIP via the PRODUCTION selection entry (mirror of afl.Cosmetic.SetFacemask) -> gate passes (owned) -> refresh.
			FAFLCosmeticSelection Request = L->GetSelection();
			if (Request.GetActiveIdentityId() == NAME_None)
			{
				Request.IdentityType = EAFLIdentityType::Team;
				Request.TeamId = FName(TEXT("AFL.Team.ARIA"));
			}
			Request.FacemaskId = VisorId;
			L->ServerSetCosmeticSelection(Request); // PURE: client-issued; server gates (owned) + drives the refresh.

			UE_LOG(LogTemp, Display, TEXT("AFL_TEST[VISOR] equipped -- ServerSetCosmeticSelection(facemask=%s) issued through the production seam; entitlement gate passes because owned."), *VisorId.ToString());
			UE_LOG(LogTemp, Display, TEXT("AFL_TEST[VISOR] render-refreshed -- the runtime path swaps the hero slot-1 facemask material. WATCH the hero's visor change to the Japan flag (visually distinct from base default). Use `afl.SkinDiag 1` to log RefreshFacemask/OnRep_Facemask/ApplyFacemask."));
			if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green, TEXT("[VISOR] purchased+granted+equipped -- WATCH the facemask swap to Flag.Japan")); }
		});
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLOnlineVerifyVisorCanaryCmd(TEXT("afl.Online.VerifyVisorCanary"),
		TEXT("Phase-2 Visors canary: buy AFL.Facemask.Flag.Japan (5000 VO) via the PRODUCTION entry ClientRequestPurchase -> Phase-1 seam -> PlayFab, then equip via ServerSetCosmeticSelection -> runtime facemask render-swap. Proves buy->entitled->equip->render on a real seeded SKU. AFL_TEST[VISOR] purchased/granted/equipped/render-refreshed -- WATCH the visible swap. Run in PIE with account VO>=5000 and Flag.Japan not-yet-owned."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLOnlineVerifyVisorCanary));
#endif // !UE_BUILD_SHIPPING

	// --- SKINTEST one-command 6-gate driver: afl.SkinTest.RunAll ------------------
	// Sequences the EXISTING cheats (Grant/SetBody/SetEdge/SetFacemask) with timed FTimerHandle pauses + [SKINTEST]
	// log + on-screen markers, so the operator runs ONE command and just WATCHES each gate. Calls the named handlers
	// directly (same file) -> no console-routing risk; each gets the captured host World + *GLog. RUN ON THE HOST
	// (listen-server) WINDOW: Grant is authority-side (DebugGrantOwnership) and SetBody/SetEdge commit locally there;
	// the host is also a client so it watches the result. GATE 3 (no-selection brand default) runs FIRST: there is
	// NO body/edge CLEAR cheat (the commit only sets non-None), so the genuine no-selection state is the FRESH SPAWN
	// -- read it before any SetBody/SetEdge. GATE 5 race-converge is the only 2-client item (logged, not automatable).
	static const TCHAR* const GSkinTestGap[6] = { TEXT("NeonYellow"), TEXT("Crimson"), TEXT("Indigo"), TEXT("Solar"), TEXT("Magenta"), TEXT("Lime") };
	static TWeakObjectPtr<UWorld> GSkinTestWorld;
	static int32 GSkinTestStep = 0;
	static FTimerHandle GSkinTestTimer;

	static void SkinTest_Mark(const FString& Line)
	{
		// LogAFLCombat (already visible here, always-on at Display) so the gate markers show regardless of
		// afl.SkinDiag -- which step 0 enables separately to surface the [SkinDiag] resolve internals alongside.
		UE_LOG(LogAFLCombat, Display, TEXT("[SKINTEST] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Yellow, FString::Printf(TEXT("[SKINTEST] %s"), *Line));
		}
	}

	static void SkinTest_Body(const TCHAR* Color)
	{
		UWorld* W = GSkinTestWorld.Get();
		if (!W) { return; }
		HandleAFLWalletGrant(TArray<FString>{ FString::Printf(TEXT("AFL.Body.%s"), Color) }, W, *GLog); // entitle the axis
		HandleAFLCosmeticSetBody(TArray<FString>{ FString(Color) }, W, *GLog);
	}

	static void SkinTest_Edge(const TCHAR* Color)
	{
		UWorld* W = GSkinTestWorld.Get();
		if (!W) { return; }
		HandleAFLWalletGrant(TArray<FString>{ FString::Printf(TEXT("AFL.Edge.%s"), Color) }, W, *GLog);
		HandleAFLCosmeticSetEdge(TArray<FString>{ FString(Color) }, W, *GLog);
	}

	static void SkinTest_Advance();

	static void SkinTest_Wait(float Seconds)
	{
		if (UWorld* W = GSkinTestWorld.Get())
		{
			W->GetTimerManager().SetTimer(GSkinTestTimer, FTimerDelegate::CreateStatic(&SkinTest_Advance), Seconds, false);
		}
	}

	static void SkinTest_Advance()
	{
		UWorld* W = GSkinTestWorld.Get();
		if (!W) { return; }
		const int32 S = GSkinTestStep++;
		switch (S)
		{
		case 0:
			if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.SkinDiag"))) { CVar->Set(TEXT("1")); }
			SkinTest_Mark(TEXT("START (run on the HOST/listen-server window). SkinDiag ON. ~32s, 6 gates."));
			SkinTest_Wait(1.5f);
			break;
		case 1: // GATE 3 first = genuine no-selection (fresh-spawn) brand default
			SkinTest_Mark(TEXT("GATE3 BRAND-DEFAULT: no selection (fresh spawn) -> WATCH: IRONICS RED body. BLOCK if grey/default. (Fresh PIE = clean read.)"));
			SkinTest_Wait(4.0f);
			break;
		case 2: // GATE 1 MIX-AND-MATCH (the core proof)
			SkinTest_Body(TEXT("NeonPurple"));
			SkinTest_Edge(TEXT("NeonGreen"));
			SkinTest_Mark(TEXT("GATE1 MIX: body=NeonPurple edge=NeonGreen -> WATCH: PURPLE body + GREEN edge, independent. The core proof."));
			SkinTest_Wait(4.0f);
			break;
		case 3: // GATE 2 UNIFIED LIME
			SkinTest_Body(TEXT("Lime"));
			SkinTest_Edge(TEXT("Lime"));
			SkinTest_Mark(TEXT("GATE2 LIME: body+edge=Lime (incl created Finish_Lime) -> WATCH: full LIME identity."));
			SkinTest_Wait(4.0f);
			break;
		case 4: // GATE 4 FACEMASK (can BLOCK)
		{
			HandleAFLWalletGrant(TArray<FString>{ FString(TEXT("AFL.Facemask.JapanSolar")) }, W, *GLog);
			HandleAFLCosmeticSetFacemask(TArray<FString>{ FString(TEXT("JapanSolar")) }, W, *GLog);
			SkinTest_Body(TEXT("NeonPurple"));
			SkinTest_Mark(TEXT("GATE4 FACEMASK: visor=JapanSolar body=NeonPurple -> WATCH: visor shows ITS design color, NOT purple. BLOCK if it bleeds."));
			SkinTest_Wait(4.0f);
			break;
		}
		case 5: // GATE 5 RACE (2-client item)
			SkinTest_Mark(TEXT("GATE5 RACE: needs 2 clients (manual/deferred). Server resolved + replicated above; watch a 2nd client mirror the host body. Single-client cannot prove converge."));
			SkinTest_Wait(2.0f);
			break;
		case 6: case 7: case 8: case 9: case 10: case 11: // GATE 6 GAP COLORS (6)
		{
			const int32 G = S - 6;
			SkinTest_Body(GSkinTestGap[G]);
			SkinTest_Mark(FString::Printf(TEXT("GAP %d/6: body=%s -> WATCH: body takes its registry TeamColor."), G + 1, GSkinTestGap[G]));
			SkinTest_Wait(2.0f);
			break;
		}
		default:
			HandleAFLCosmeticSetFacemask(TArray<FString>{ FString(TEXT("none")) }, W, *GLog); // tidy: un-equip the test visor
			SkinTest_Mark(TEXT("DONE. Report per gate: PASS or BLOCK (esp. #3 grey, #4 bleed)."));
			GSkinTestWorld.Reset();
			break;
		}
	}

	void HandleAFLSkinTestRunAll(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.SkinTest.RunAll - run inside PIE (the HOST/listen-server window).")); return; }
		GSkinTestWorld = World;
		GSkinTestStep = 0;
		Ar.Log(TEXT("afl.SkinTest.RunAll - starting the 6-gate Option B watch. Watch the screen + [SKINTEST] markers. ~32s. (Run on the HOST window; GATE5 race wants a 2nd client.)"));
		SkinTest_Advance();
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLSkinTestRunAllCmd(
		TEXT("afl.SkinTest.RunAll"),
		TEXT("One-command Option B body-axis watch: SkinDiag + grant + SetBody/SetEdge/SetFacemask sequenced across the 6 gates with timed pauses + [SKINTEST] markers. Run on the HOST window; watch each gate. GATE5 race needs 2 clients (flagged)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLSkinTestRunAll));

	// --- FANATICS PILOT 4-check driver: afl.Cosmetic.PilotCheck ---------------------
	// Automates the un-called unique-body pilot checks (P1 facemask BOTH ways / P2 sever the
	// operator's OWN arm / P3 body+edge finish push -- PREDICTED param-family NO-OP on a unique
	// body, the fleet gate) with timed pauses + AFL_TEST[PILOT] markers so the operator only
	// WATCHES. Same FTimerHandle sequencer shape as afl.SkinTest.RunAll above (HOST window,
	// quiet spot, wearing FANATICS via afl.Cosmetic.SetIdentity AFL.Team.FANATICS + respawn).
	// P2 rides the EXISTING AFL.Dismember.TestSeverSelf cheat via console exec -- cross-module
	// by string, no AFLDismember link dep; it no-ops harmlessly if cheats are compiled out.
	static TWeakObjectPtr<UWorld> GPilotCheckWorld;
	static int32 GPilotCheckStep = 0;
	static FTimerHandle GPilotCheckTimer;

	static void PilotCheck_Mark(const FString& Line)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[PILOT] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 7.0f, FColor::Cyan, FString::Printf(TEXT("AFL_TEST[PILOT] %s"), *Line));
		}
	}

	static void PilotCheck_Advance();

	static void PilotCheck_Wait(float Seconds)
	{
		if (UWorld* W = GPilotCheckWorld.Get())
		{
			W->GetTimerManager().SetTimer(GPilotCheckTimer, FTimerDelegate::CreateStatic(&PilotCheck_Advance), Seconds, false);
		}
	}

	static void PilotCheck_Advance()
	{
		UWorld* W = GPilotCheckWorld.Get();
		if (!W) { return; }
		const int32 S = GPilotCheckStep++;
		switch (S)
		{
		case 0:
			PilotCheck_Mark(TEXT("START -- FANATICS pilot checks. HOST window, quiet corner, wearing FANATICS. ~25s."));
			PilotCheck_Wait(2.0f);
			break;
		case 1: // P1a FACEMASK ON (grant + equip via the proven slot-1 swap path)
			HandleAFLWalletGrant(TArray<FString>{ FString(TEXT("AFL.Facemask.Kawaii")) }, W, *GLog);
			HandleAFLCosmeticSetFacemask(TArray<FString>{ FString(TEXT("Kawaii")) }, W, *GLog);
			PilotCheck_Mark(TEXT("P1a FACEMASK ON (Kawaii) -> WATCH: the visor region swaps to the mask design."));
			PilotCheck_Wait(4.0f);
			break;
		case 2: // P1b FACEMASK OFF -> authored slot-1 must RESTORE
			HandleAFLCosmeticSetFacemask(TArray<FString>{ FString(TEXT("none")) }, W, *GLog);
			PilotCheck_Mark(TEXT("P1b FACEMASK OFF -> WATCH: the visor RESTORES to the authored FANATICS look. BOTH directions must take."));
			PilotCheck_Wait(4.0f);
			break;
		case 3: // P2 SEVER the local player's own right arm (existing self-sever cheat)
			if (GEngine) { GEngine->Exec(W, TEXT("AFL.Dismember.TestSeverSelf upperarm_r")); }
			PilotCheck_Mark(TEXT("P2 SEVER own upperarm_r -> WATCH: the FANATICS arm VANISHES + a gib drops (grab to reattach). Log: [AFLDismember] ZONE SEVER."));
			PilotCheck_Wait(5.0f);
			break;
		case 4: // P3a BODY finish push. NOT via SkinTest_Body -- that helper reads GSkinTestWorld
			// (set only by afl.SkinTest.RunAll) and silently no-ops here (log-proven 2026-07-23).
			// Call the handlers directly with OUR world.
			HandleAFLWalletGrant(TArray<FString>{ FString(TEXT("AFL.Body.NeonBlue")) }, W, *GLog);
			HandleAFLCosmeticSetBody(TArray<FString>{ FString(TEXT("NeonBlue")) }, W, *GLog);
			PilotCheck_Mark(TEXT("P3a BODY=NeonBlue pushed -> PREDICTED NO-OP on the unique body (param-family gate). WATCH for ANY change."));
			PilotCheck_Wait(4.0f);
			break;
		case 5: // P3b EDGE finish push (direct handlers, same fix)
			HandleAFLWalletGrant(TArray<FString>{ FString(TEXT("AFL.Edge.NeonGreen")) }, W, *GLog);
			HandleAFLCosmeticSetEdge(TArray<FString>{ FString(TEXT("NeonGreen")) }, W, *GLog);
			PilotCheck_Mark(TEXT("P3b EDGE=NeonGreen pushed -> PREDICTED NO-OP (team-edge params absent on the unique-body master). WATCH."));
			PilotCheck_Wait(4.0f);
			break;
		default:
			PilotCheck_Mark(TEXT("DONE. Report: P1 swap+restore / P2 arm-vanish+gib / P3 no-op-or-changed. (P4 colorways is editor-side.)"));
			GPilotCheckWorld.Reset();
			break;
		}
	}

	void HandleAFLCosmeticPilotCheck(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Cosmetic.PilotCheck - run inside PIE (the HOST window), wearing FANATICS.")); return; }
		GPilotCheckWorld = World;
		GPilotCheckStep = 0;
		Ar.Log(TEXT("afl.Cosmetic.PilotCheck - starting the FANATICS pilot check watch (~25s). Watch the AFL_TEST[PILOT] markers. P4 colorways is editor-side."));
		PilotCheck_Advance();
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticPilotCheckCmd(
		TEXT("afl.Cosmetic.PilotCheck"),
		TEXT("FANATICS unique-body pilot check driver: P1 facemask on/off (slot-1 swap+restore) + P2 sever own arm (via AFL.Dismember.TestSeverSelf) + P3 body/edge finish push (PREDICTED no-op on a unique body -- the fleet gate), timed with AFL_TEST[PILOT] markers. HOST window, wearing FANATICS, quiet spot."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticPilotCheck));

	// ─── AUTOMATED readability/variety test: afl.Cosmetic.Test.Readability ────────
	//
	// ONE server-authoritative command that configures the WHOLE visual test so the
	// operator only has to WATCH (no per-window manual cheats): every player gets an
	// OPPOSING gameplay LyraTeam (so per-viewer enemy nameplates differ) + a DISTINCT
	// finish (so the bodies read apart). Because it runs server-side, the entitlement
	// grant lands for EVERY player -- the client-side afl.Wallet.Grant cannot (no
	// authority on a client). Run it in the LISTEN-SERVER / host window. Repeatable.
	void HandleAFLCosmeticTestReadability(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Cosmetic.Test.Readability - run inside PIE.")); return; }
		if (World->GetNetMode() == NM_Client) { Ar.Log(TEXT("afl.Cosmetic.Test.Readability - run on the LISTEN-SERVER/host window (server-authoritative).")); return; }

		ULyraTeamSubsystem* TeamSub = UWorld::GetSubsystem<ULyraTeamSubsystem>(World);
		TArray<int32> TeamIds = TeamSub ? TeamSub->GetTeamIDs() : TArray<int32>();
		TeamIds.Sort();

		// Distinct finishes (all four resolve through DA_AFL_BrandEdgeMap; NOT NeonRed -- absent).
		static const FName Edges[] = {
			FName(TEXT("AFL.Edge.NeonBlue")),  FName(TEXT("AFL.Edge.NeonGreen")),
			FName(TEXT("AFL.Edge.NeonPink")),  FName(TEXT("AFL.Edge.NeonPurple")) };

		int32 PlayerIdx = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || !PC->PlayerState) { continue; }
			APlayerState* PS = PC->PlayerState;

			// 1) Opposing GAMEPLAY team (round-robin over the existing team ids; needs >= 2 teams to differ).
			int32 AssignedTeam = -1;
			if (TeamSub && TeamIds.Num() >= 2)
			{
				AssignedTeam = TeamIds[PlayerIdx % TeamIds.Num()];
				TeamSub->ChangeTeamForActor(PC, AssignedTeam);
			}

			// 2) Distinct FINISH: grant (server authority -> works for EVERY player) then commit the selection.
			const FName Edge = Edges[PlayerIdx % UE_ARRAY_COUNT(Edges)];
			if (UAFLWalletComponent* Wallet = PS->FindComponentByClass<UAFLWalletComponent>())
			{
				Wallet->DebugGrantOwnership(Edge);
			}
			if (UAFLCosmeticLoadoutComponent* Loadout = PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>())
			{
				FAFLCosmeticSelection Sel = Loadout->GetSelection();
				if (Sel.GetActiveIdentityId() == NAME_None)
				{
					Sel.IdentityType = EAFLIdentityType::Team;
					Sel.TeamId = FName(TEXT("AFL.Team.IRONICS"));
				}
				Sel.EdgeId = Edge;
				Loadout->ServerSetCosmeticSelection(Sel);
			}

			Ar.Logf(TEXT("[AFL_TEST_READABILITY] player %d (%s): gameplayTeam=%d finish=%s"),
				PlayerIdx, *PS->GetName(), AssignedTeam, *Edge.ToString());
			++PlayerIdx;
		}
		Ar.Logf(TEXT("[AFL_TEST_READABILITY] configured %d player(s). WATCH: distinct body finishes + per-viewer enemy nameplates. ")
			TEXT("Logs: [AFL_TEST_READABILITY] (team+finish per player) and [W_Nameplate_C] (TeamId each nameplate received)."),
			PlayerIdx);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticTestReadabilityCmd(TEXT("afl.Cosmetic.Test.Readability"),
		TEXT("AUTOMATED readability/variety test (server-auth, run on host): give every player an OPPOSING gameplay team + a DISTINCT finish, then WATCH (distinct bodies + per-viewer enemy nameplates). Repeatable; replaces the manual per-window cheat dance."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCosmeticTestReadability));

	// ─── S-ECON-STORE: afl.Store.Open / afl.Store.Close (push the cosmetic store onto UI.Layer.Menu) ──
	//
	// The store is a ULyraActivatableWidget (AFLW_Menu_CosmeticShop). Opening it = pushing it onto the
	// CommonUI "UI.Layer.Menu" stack for the LOCAL player of the window the command was typed in (world-
	// context-aware, like afl.Cosmetic.SetEdge). PushContentToLayer_ForPlayer is the SAME call Lyra's HUD
	// layout uses for its menus -- the widget's InputConfig=Menu then auto-captures input, and the
	// CloseButton's DeactivateWidget pops it off this same stack. The store's own event graph drives all
	// catalog/wallet reads; this cheat only summons it. Cosmetic/UI-only -> run in the window you watch.
	//
	// NOT a GameFeatureAction_AddWidget: that would mount the store always-on in the HUD; the store is a
	// summoned modal, so a push-on-demand (cheat now; a HUD button later) is the correct shape.
	// Weak handle to the store widget the last afl.Store.Open pushed, so afl.Store.Close can pop
	// exactly it (UCommonUIExtensions has no "active widget on layer" getter — PopContentFromLayer
	// takes the widget pointer). Weak so a player-driven close (the X button) doesn't leave a stale
	// raw pointer; we null-check before popping.
	TWeakObjectPtr<UCommonActivatableWidget> GAFLStoreWidget;

	void HandleAFLStoreOpen(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Store.Open - run inside PIE.")); return; }
		APlayerController* PC = World->GetFirstPlayerController();
		ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
		if (!LP) { Ar.Log(TEXT("afl.Store.Open - no local player.")); return; }

		TSubclassOf<UCommonActivatableWidget> StoreClass = LoadClass<UCommonActivatableWidget>(
			nullptr, TEXT("/Game/BagMan/UI/Store/AFLW_Menu_CosmeticShop.AFLW_Menu_CosmeticShop_C"));
		if (!StoreClass)
		{
			Ar.Log(TEXT("afl.Store.Open - could not load AFLW_Menu_CosmeticShop_C (build the widget / check the path)."));
			return;
		}

		UCommonActivatableWidget* Pushed =
			UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, TAG_UI_Layer_Menu_Store_Cheats, StoreClass);
		GAFLStoreWidget = Pushed;
		Ar.Logf(TEXT("afl.Store.Open - pushed %s onto UI.Layer.Menu (%s). Close with the X button or afl.Store.Close."),
			*StoreClass->GetName(), Pushed ? TEXT("ok") : TEXT("push returned null"));
	}

	void HandleAFLStoreClose(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		// Pop the store this cheat opened (mirrors the CloseButton's DeactivateWidget). If the player
		// already closed it via the X, the weak handle is stale -> nothing to do.
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Store.Close - run inside PIE.")); return; }
		if (UCommonActivatableWidget* Store = GAFLStoreWidget.Get())
		{
			UCommonUIExtensions::PopContentFromLayer(Store);
			GAFLStoreWidget = nullptr;
			Ar.Log(TEXT("afl.Store.Close - popped the cosmetic store from UI.Layer.Menu."));
		}
		else
		{
			Ar.Log(TEXT("afl.Store.Close - store not open (or already closed via the X button)."));
		}
	}

	// ─── STEP 5: afl.Market.Loadout -- push the SAME market widget in LOADOUT mode (owned cosmetics + EQUIP onto the
	// armory display robot). Unlike afl.Store.Open, Mode must be set BEFORE NativeConstruct, so we push via the
	// PrimaryGameLayout init-hook (PushWidgetToLayerStack) rather than PushContentToLayer_ForPlayer. Same widget
	// class, same UI.Layer.Menu; reuses GAFLStoreWidget so afl.Store.Close pops it too.
	void HandleAFLLoadoutOpen(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Market.Loadout - run inside PIE.")); return; }
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC) { Ar.Log(TEXT("afl.Market.Loadout - no player controller.")); return; }
		UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(PC);
		if (!Layout) { Ar.Log(TEXT("afl.Market.Loadout - no PrimaryGameLayout for this player.")); return; }

		TSubclassOf<UCommonActivatableWidget> MarketClass = LoadClass<UCommonActivatableWidget>(
			nullptr, TEXT("/Game/BagMan/UI/Store/AFLW_Menu_CosmeticShop.AFLW_Menu_CosmeticShop_C"));
		if (!MarketClass) { Ar.Log(TEXT("afl.Market.Loadout - could not load AFLW_Menu_CosmeticShop_C.")); return; }

		UAFLW_FrontEndMarket* Pushed = Layout->PushWidgetToLayerStack<UAFLW_FrontEndMarket>(
			TAG_UI_Layer_Menu_Store_Cheats, MarketClass,
			[](UAFLW_FrontEndMarket& W) { W.Mode = EAFLMarketMode::Loadout; });
		GAFLStoreWidget = Pushed;
		if (Pushed)
		{
			Pushed->EnterLoadout(); // CommonUI runs the init-hook AFTER construct -> enter LOADOUT explicitly here.
		}
		Ar.Logf(TEXT("afl.Market.Loadout - pushed the market in LOADOUT mode (%s). Close with the X or afl.Store.Close."),
			Pushed ? TEXT("ok") : TEXT("push returned null"));
	}
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLoadoutOpenCmd(TEXT("afl.Market.Loadout"),
		TEXT("STEP 5: push the market in LOADOUT mode (owned cosmetics + equip onto the armory display robot)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLLoadoutOpen));

	// ===========================================================================================
	//  EOS-AUTH-C2 cheats (Track-2 EOS auth/friends lane; builds on C1's platform+Connect proof).
	//  These read the OSSv2 UE::Online path (DefaultServices=Epic), the same interface Lyra's
	//  CommonUser uses. afl.EOS.Auth.Status is the value-check instrument (proves the EAS login's
	//  EpicAccountId, distinct from C1's anonymous Connect PUID); afl.EOS.Friends.Query is the
	//  thin friends-plumbing proof (N=0 is a valid green).
	// ===========================================================================================
	using namespace UE::Online;

	// Resolve the Epic OSSv2 services + the local auth account (UserIdx 0). Returns null if EOS
	// isn't the active services or no platform-user 0 is logged in.
	IOnlineServicesPtr GetEpicServices()
	{
		return GetServices(EOnlineServices::Epic);
	}

	// The local account id of platform user 0 (the one the Developer auth logs in), if logged in.
	bool GetLocalEOSAccountId(FAccountId& OutAccountId, FString& OutReason)
	{
		IOnlineServicesPtr Services = GetEpicServices();
		if (!Services)            { OutReason = TEXT("no Epic OnlineServices (is this the LyraGameEOS / -CustomConfig=EOS run?)"); return false; }
		IAuthPtr Auth = Services->GetAuthInterface();
		if (!Auth)                { OutReason = TEXT("no Auth interface"); return false; }
		// Platform user 0 is the local player the Developer-auth CLI logs in.
		const FPlatformUserId User = FPlatformUserId::CreateFromInternalId(0);
		TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> R =
			Auth->GetLocalOnlineUserByPlatformUserId({ User });
		if (R.IsError())          { OutReason = FString::Printf(TEXT("not logged in (%s)"), *R.GetErrorValue().GetLogString()); return false; }
		OutAccountId = R.GetOkValue().AccountInfo->AccountId;
		return true;
	}

	// afl.EOS.Auth.Status -- dump the EOS auth state. The value-check that distinguishes the EAS
	// login (EpicAccountId) from C1's Connect-only result.
	void HandleAFLEOSAuthStatus(const TArray<FString>& /*Args*/, UWorld* /*World*/, FOutputDevice& Ar)
	{
		FAccountId AccountId;
		FString Reason;
		if (!GetLocalEOSAccountId(AccountId, Reason))
		{
			Ar.Logf(TEXT("AFL_EOS auth: NOT logged in -- %s"), *Reason);
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_EOS auth: EpicAccountId=none (%s)"), *Reason);
			return;
		}
		// ToLogString prints the resolved account id; a non-empty/valid id here is the EAS proof.
		const FString IdStr = ToLogString(AccountId);
		Ar.Logf(TEXT("AFL_EOS auth: logged in -- AccountId=%s"), *IdStr);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_EOS auth: EpicAccountId/AccountId=%s (EAS login proven if non-empty)"), *IdStr);
	}

	// afl.EOS.Friends.Query -- query the OSSv2 Social interface friends list and log the count.
	// N=0 is a valid plumbing-proven green (proves the interface is wired + the EpicAccountId valid).
	void HandleAFLEOSFriendsQuery(const TArray<FString>& /*Args*/, UWorld* /*World*/, FOutputDevice& Ar)
	{
		FAccountId AccountId;
		FString Reason;
		if (!GetLocalEOSAccountId(AccountId, Reason))
		{
			Ar.Logf(TEXT("AFL_EOS friends: query=FAIL (no login) -- %s"), *Reason);
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_EOS friends: query=FAIL, no EpicAccountId (%s)"), *Reason);
			return;
		}
		IOnlineServicesPtr Services = GetEpicServices();
		ISocialPtr Social = Services ? Services->GetSocialInterface() : nullptr;
		if (!Social)
		{
			Ar.Log(TEXT("AFL_EOS friends: query=FAIL -- no Social interface"));
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_EOS friends: query=FAIL, no Social interface"));
			return;
		}
		Ar.Log(TEXT("AFL_EOS friends: query issued (async) -- result follows in the log on completion."));
		Social->QueryFriends({ AccountId }).OnComplete([AccountId](const TOnlineResult<FQueryFriends>& Result)
		{
			if (Result.IsError())
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_EOS friends: query=FAIL -- %s"), *Result.GetErrorValue().GetLogString());
				return;
			}
			// Query succeeded; read the cached friends list for the count.
			IOnlineServicesPtr Svc = GetServices(EOnlineServices::Epic);
			ISocialPtr Soc = Svc ? Svc->GetSocialInterface() : nullptr;
			int32 N = -1;
			if (Soc)
			{
				TOnlineResult<FGetFriends> GR = Soc->GetFriends({ AccountId });
				if (GR.IsOk()) { N = GR.GetOkValue().Friends.Num(); }
			}
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_EOS friends: query=OK, N=%d"), N);
		});
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStoreOpenCmd(TEXT("afl.Store.Open"),
		TEXT("S-ECON-STORE: push the cosmetic store (AFLW_Menu_CosmeticShop) onto UI.Layer.Menu for the local player. Browse priced/tiered catalog; buy routes through the proven wallet ServerPurchaseCosmetic."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStoreOpen));
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStoreCloseCmd(TEXT("afl.Store.Close"),
		TEXT("S-ECON-STORE: pop the top widget on UI.Layer.Menu (closes the store if open). Same effect as the store's X button."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStoreClose));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLEOSAuthStatusCmd(TEXT("afl.EOS.Auth.Status"),
		TEXT("EOS-AUTH-C2: dump the OSSv2 Epic auth state -- AFL_EOS auth: AccountId=<id|none>. A non-empty id proves the EAS (EpicAccountId) login, distinct from C1's anonymous Connect PUID. Run in the LyraGameEOS -CustomConfig=EOS run after the Developer auth login."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLEOSAuthStatus));
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLEOSFriendsQueryCmd(TEXT("afl.EOS.Friends.Query"),
		TEXT("EOS-AUTH-C2: query the OSSv2 Social friends list -> AFL_EOS friends: query=<OK|FAIL>, N=<count>. N=0 is a valid plumbing-proven green (interface wired + EpicAccountId valid). Requires the EAS login (run afl.EOS.Auth.Status first)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLEOSFriendsQuery));
}

#endif // UE_WITH_CHEAT_MANAGER


void UAFLCombatCheats::DumpCombatAttributes()
{
#if UE_WITH_CHEAT_MANAGER
	UAbilitySystemComponent* ASC = GetPlayerASC();
	if (!ASC)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("DumpCombatAttributes: no player ASC"));
		return;
	}

	auto Read = [ASC](const FGameplayAttribute& A) { return ASC->GetNumericAttribute(A); };

	UE_LOG(LogAFLCombat, Display,
		TEXT("[Combat] H=%.1f/%.1f S=%.1f/%.1f Ar=%.1f OT=%.1f D=%.1f"),
		Read(ULyraHealthSet::GetHealthAttribute()),          // CONVERGENCE: real Health lives on the Lyra set now
		Read(ULyraHealthSet::GetMaxHealthAttribute()),
		Read(UAFLAttributeSet_Combat::GetShieldAttribute()),
		Read(UAFLAttributeSet_Combat::GetMaxShieldAttribute()),
		Read(UAFLAttributeSet_Combat::GetArmorAttribute()),
		Read(UAFLAttributeSet_Combat::GetOverkillThresholdAttribute()),
		Read(UAFLAttributeSet_Combat::GetDamageAttribute()));
#endif
}

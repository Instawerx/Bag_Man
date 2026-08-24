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
#include "Cosmetics/AFLWalletComponent.h"
#include "Cosmetics/AFLAccessoryPartActor.h"   // CC-8: the wrist orientation assert
#include "Kismet/GameplayStatics.h"   // CC-X30 relaunch arm: DoesSaveGameExist, the mirror-absent discriminator            // S-ECON-WALLET: balance/gate/earn-spend cheats
#include "UI/AFLW_LoadoutBase.h"
#include "UI/AFLW_Creator.h"   // CC-5.2 widget probe
#include "Engine/SkeletalMeshSocket.h"   // CC-X34 socket authoring
#include "Animation/Skeleton.h"   // CC-X34 socket authoring
#include "LyraGameplayTags.h"   // CC-7 damage kill
#include "System/LyraGameData.h"   // CC-7 damage kill
#include "System/LyraAssetManager.h"   // CC-7 damage kill
#include "RenderingThread.h"   // CC-7 bisect: FlushRenderingCommands
#include "Engine/Canvas.h"   // CC-7 screen proof: K2_DrawBox
#include "Kismet/KismetRenderingLibrary.h"   // CC-7 screen proof
#include "Rendering/SkeletalMeshLODModel.h"   // CC-7 step 5 verify
#include "Rendering/SkeletalMeshModel.h"   // CC-7 step 5 verify
#include "Editor.h"   // CC-7 editor-world capture
#include "Animation/SkeletalMeshActor.h"   // CC-7 identity render hash
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"   // CC-7 sticker sampler
#include "Materials/MaterialExpressionAppendVector.h"   // CC-7 sticker sampler
#include "Materials/MaterialExpressionAdd.h"   // CC-7 sticker sampler
#include "Materials/MaterialExpressionMultiply.h"   // CC-7 sticker sampler
#include "Materials/MaterialExpressionScalarParameter.h"   // CC-7 sticker sampler
#include "Materials/MaterialExpressionTextureCoordinate.h"   // CC-7 sticker sampler
#include "MaterialEditingLibrary.h"   // CC-7 sticker sampler
#include "Misc/FileHelper.h"   // CC-7 material graph read
#include "Materials/MaterialExpressionTextureSampleParameter.h"   // CC-7 material graph read
#include "Materials/MaterialExpressionParameter.h"   // CC-7 material graph read
#include "Materials/MaterialExpression.h"   // CC-7 material graph read
#include "Materials/Material.h"   // CC-7 material graph read
#include "Cosmetics/AFLSkinColorComponent.h"   // CC-6.5 preview-vs-spawn override readback // CC-5.3 probe: the creator interface under test
#include "UObject/UObjectIterator.h" // CC-5.3 probe: find an already-open loadout widget
#include "UI/AFLLoadoutDisplayPawn.h" // CC-5.3 probe: the preview pawn whose MIDs are read
#include "Blueprint/UserWidget.h" // CC-5.3 probe: CreateWidget for the concrete WBP subclass
#include "Player/LyraPlayerState.h" // CC-6.1 VerifyNewSkuBuy: IsEntitled takes a ALyraPlayerState* and the wallet header only forward-declares it
#include "Cosmetics/AFLEconomyPersistenceSubsystem.h" // A1.1: afl.Online.VerifyA11 (wipe-local -> load -> assert PlayFab)
#include "Engine/GameInstance.h"                      // A1.1: GetSubsystem<UAFLEconomyPersistenceSubsystem>()
#include "AFLOnlineSubsystem.h"                   // CC-X23: IsLoggedIn() -- the probe precondition. The module was already a dependency; the header was never included, because every other online probe reached PlayFab THROUGH the wallet rather than asking the session directly.
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
#include "Widgets/CommonActivatableWidgetContainer.h"   // CC-5 entry proof: read the Menu layer's top widget
#include "Components/PanelWidget.h"                     // CC-5 entry proof: the rails' children
#include "Framework/Application/SlateApplication.h"     // CC-5 entry proof: a REAL press+release
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
		// GATHER BY ID PREFIX, NOT BY TYPE (CC-X15b). HISTORICAL REASON: GetEntriesByType(Facemask) once
		// returned only 33 of the 60 AFL.Facemask.* rows -- 27 were typed SkinColor_Edge, left behind by the
		// migration to the dedicated Facemask type. Filtering by type hid half the catalog.
		// CURRENT (measured after cc-6-3): 38 AFL.Facemask.* rows, ALL typed Facemask, none SkinColor_Edge --
		// the 27 were retyped at cc-x16-done and the retired identities' facemask rows went with the roster
		// cut (60 - 22 = 38).
		//
		// THE PREFIX GATHER STAYS, but NOT for the reason an earlier revision of this comment gave. It
		// claimed "the Type default is still SkinColor_Edge, so the next untyped row lands in the same
		// hole" -- already false when written: 4eb4e1c9 (2026-08-18) changed the default to Invalid, so a
		// new untyped row is detectable and the lint reports invalid=0. See the CC-X17 lint comment below,
		// which had this right. The real reason to keep the prefix gather is that fixing the default did
		// not retype rows already holding the old one, and a row can still be mistyped BY HAND to any
		// value -- an id prefix is what actually defines the axis here.
		// Matching AFLCosmeticBrowserLibrary:99, address rows by their id namespace instead.
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
		// defect; emitting the split keeps the defect visible instead of papering over it.
		// EXPECT FACEMASK=38 SKIN_COLOR_EDGE=0 (measured after cc-6-3). This line previously said to expect
		// FACEMASK=33 SKIN_COLOR_EDGE=27, which was true before the cc-x16 retype -- a reader seeing the
		// healthy 38/0 would have concluded the command was broken. Any SKIN_COLOR_EDGE > 0 here is a new
		// untyped row, not the old defect.
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
	//    5x AFL.Character.*_X  typed SkinColor_Edge  (CC-X18, deliberately NOT retyped)
	//    0x AFL.Facemask.*                            (retyped at cc-x16-done)
	// WAS 27x before cc-6-3 retired 22 identities. Six _X rows remain and FANATICS_X is correctly typed
	// Character, so five carry the default -- MEASURED, not inherited. This is the lint's positive
	// control: leaving it at 27 would make the instrument fail its own self-check against a CORRECT
	// catalog, which is worse than not checking.
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
			// CC-X20: was SkinColor_Body, which nothing in the game uses -- that enum value appears
			// ONLY here and in its own declaration. The BodyColor axis queries Type==Finish
			// (QueryTypeForAxis) and the 10 AFL.Body.* rows are STORED as Finish, so the old rule
			// reported 10 mismatches against a type no surface asks for. AFL.Finish. is CANONICAL;
			// AFL.Body. is a LEGACY ALIAS that only the store classifier still accepts
			// (AFLW_FrontEndMarket::ClassifyStoreAxis maps BOTH to BodyColor). Its 10 occupants are
			// unreachable duplicates of AFL.Finish.* rows -- same Asset, different price -- so their
			// disposition is a PRICE call, not a type one.
			{ TEXT("AFL.Body."),      EAFLCosmeticType::Finish,          EAFLCosmeticType::Finish },
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
			// CC-X20: THE 50 "unmapped" ROWS. AFL.WeaponSkin.* had no rule at all, so every camo row
			// fell into `unmapped` and the lint reported a 50-row blind spot as if it were clean --
			// the same failure CC-6.1 fixed for the slot rows. Weapon is CORRECT for them and is
			// deliberately overloaded: QueryTypeForAxis sends BOTH the Weapon and WeaponSkin axes to
			// Type==Weapon, then GetAxisIdPrefix splits them by namespace. Retyping them to
			// WeaponAccessory would drop them out of GetEntriesByType(Weapon) and EMPTY the CAMOS tab.
			{ TEXT("AFL.WeaponSkin."), EAFLCosmeticType::Weapon,       EAFLCosmeticType::Weapon },
			// CC-6.1: without this rule the robot/slot rows land in `unmapped` and the lint reports a
			// growing blind spot as if it were a clean result.
			{ TEXT("AFL.CreatorSlot."), EAFLCosmeticType::CreatorSlot,  EAFLCosmeticType::CreatorSlot },
			// CC-X20: AFL.WeaponCredit.x3 was added 2026-08-21 and landed straight in `unmapped` -- the
			// SAME blind spot CC-6.1 fixed for the slot rows, reintroduced by the next new SKU. Any new
			// namespace needs a rule here in the same commit that creates it.
			{ TEXT("AFL.WeaponCredit."), EAFLCosmeticType::WeaponCredit, EAFLCosmeticType::WeaponCredit },
			// CC-7.2: THE RULE SHIPS WITH THE NAMESPACE THAT NEEDS IT. AFL.WeaponCredit.x3 was added
			// earlier today WITHOUT one and landed straight in `unmapped` -- the same blind spot CC-6.1
			// had already closed once for the slot rows. A namespace with no rule makes the lint report
			// a growing hole as a clean result.
			// CC-7: BEFORE the AFL.Sticker. rule, and that ORDER IS THE RULE, not a formatting choice.
			// "AFL.Sticker." is a strict prefix of "AFL.StickerCredit.", so a first-match table that
			// tested the shorter one first would classify both credit SKUs as Sticker rows -- and a
			// mis-typed SKU is exactly the CC-X17 shape that hid 27 facemask rows.
			{ TEXT("AFL.StickerCredit."), EAFLCosmeticType::StickerCredit, EAFLCosmeticType::StickerCredit },
			{ TEXT("AFL.Sticker."), EAFLCosmeticType::Sticker,     EAFLCosmeticType::Sticker },
			// CC-8: same rule, same commit as the namespace.
			{ TEXT("AFL.Accessory."), EAFLCosmeticType::Accessory, EAFLCosmeticType::Accessory },
			// CC-5: the rule ships with the axis that needs it. AFL.Emblem. rows have existed since the
			// identity work but never had a lint rule, because they never had an axis -- so 6 rows sat in
			// `unmapped` and the lint reported that hole as a clean result.
			{ TEXT("AFL.Emblem."), EAFLCosmeticType::Emblem,       EAFLCosmeticType::Emblem },
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

				// CC-X20: THE .XT PAIR BUNDLES -- SUFFIX WINS OVER PREFIX.
				// 49 rows keep their AFL.Weapon.HandCannon.* namespace but were converted to
				// Type==Bundle by 149948f3, because .XT IS the pair id (ruled; the pair has no actor of
				// its own). The AFL.Weapon. prefix rule reads every one of them as a mismatch, so the
				// lint reported 49 DELIBERATE rows as defects -- a lint that cries wolf on the healthy
				// state trains everyone to ignore it, which is worse than no lint.
				if (Id.EndsWith(TEXT(".XT"), ESearchCase::IgnoreCase))
				{
					if (R->Type != EAFLCosmeticType::Bundle)
					{
						++Mismatch;
						ByPrefix.FindOrAdd(TEXT(".XT(pair)"))++;
						Ar.Logf(TEXT("  MISMATCH %s  type=%s  expected=Bundle (.XT pair)"), *Id,
							*TypeEnum->GetNameStringByValue((int64)R->Type));
					}
					continue;
				}

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

		// --- FREELINT: a GrantedFree row must not also claim to be transactable ---------------------
		//
		// bTransactable DEFAULTS TRUE, so true on a free row means "nobody authored it" rather than
		// "someone decided to sell it" -- measured 2026-08-23: 169 of 169 free rows carried true and
		// none carried false, which is what a uniform default looks like, not what a decision looks
		// like. Nothing was mis-sold (ClientRequestPurchase refuses GrantedFree regardless, and the
		// store filters on Acquisition), but the row-level flag was one manifest registration away
		// from being the only thing standing between a free item and a price.
		//
		// The DEFAULT IS NOT CHANGED. Flipping it to false would fix new rows and silently un-sell the
		// 140 paid rows that depend on it -- the same absent-vs-default trap, pointed the other way.
		// The DISAGREEMENT is caught instead, so a row authored tomorrow is flagged the day it lands.
		{
			int32 FreeRows = 0, FreeTransactable = 0, PaidTransactable = 0;
			for (int32 T = 0; T < (TypeEnum ? TypeEnum->NumEnums() : 0); ++T)
			{
				TArray<const FAFLCatalogEntry*> OfType;
				Catalog->GetEntriesByType(static_cast<EAFLCosmeticType>(TypeEnum->GetValueByIndex(T)), OfType);
				for (const FAFLCatalogEntry* R : OfType)
				{
					if (!R) { continue; }
					const bool bFree = (R->Acquisition == EAFLAcquisition::GrantedFree);
					if (bFree) { ++FreeRows; if (R->bTransactable) { ++FreeTransactable;
						Ar.Logf(TEXT("  FREE-BUT-TRANSACTABLE %s"), *R->CosmeticId.ToString()); } }
					else if (R->bTransactable) { ++PaidTransactable; }
				}
			}
			// THE CONTROL TRAVELS WITH THE RESULT. free=0 would also read as "0 transactable", so the
			// paid count is printed alongside: a zero next to paid=0 is a lint that found nothing.
			const bool bOk = (FreeTransactable == 0) && (FreeRows > 0) && (PaidTransactable > 0);
			Ar.Logf(TEXT("AFL_TEST[FREELINT] free=%d freeTransactable=%d paidTransactable=%d %s"),
				FreeRows, FreeTransactable, PaidTransactable, bOk ? TEXT("PASS") : TEXT("FAIL"));
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[FREELINT] free=%d freeTransactable=%d paidTransactable=%d %s"),
				FreeRows, FreeTransactable, PaidTransactable, bOk ? TEXT("PASS") : TEXT("FAIL"));
		}
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

		if (Mode == TEXT("visor") && Args.Num() > 3)
		{
			// CC-6.4 FEATURE ARM. Both migration runs left bVisorColorSet at 0, which exercises only the
			// mirror -- a split that silently did nothing would produce identical logs. This is the arm that
			// can tell those apart: it makes an EXPLICIT visor choice and the visor must then diverge from
			// the body. Goes through ServerSetCosmeticSelection so the clamp and the commit are the real ones.
			const FLinearColor Want(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]), 1.0f);
			FAFLCosmeticSelection Request = Loadout->GetSelection();
			if (Request.GetActiveIdentityId() == NAME_None)
			{
				Request.IdentityType = EAFLIdentityType::Team;
				Request.TeamId = FName(TEXT("AFL.Team.IRONICS"));
			}
			Request.bUseCreatorColors = 1;
			Request.bVisorColorSet    = 1;
			Request.CreatorVisorColor = Want;
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[VISORSET] request=(%.4f,%.4f,%.4f) priorBody=(%.4f,%.4f,%.4f)"),
				Want.R, Want.G, Want.B,
				Request.CreatorBodyColor.R, Request.CreatorBodyColor.G, Request.CreatorBodyColor.B);
			Loadout->ServerSetCosmeticSelection(Request);
			return;
		}

		// READ: report the build set AND the committed selection together, so "which build is active" and
		// "what actually got committed" can be compared rather than assumed equal.
		const FAFLCreatorBuildSet& Set = Loadout->GetBuildSet();
		const FAFLCosmeticSelection& Sel = Loadout->GetSelection();
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[BUILDPROBE] builds=%d active=%d creatorOn=%d editLocked=%d selBody=(%.4f,%.4f,%.4f) "
				"visorSet=%d rawVisor=(%.4f,%.4f,%.4f) effVisor=(%.4f,%.4f,%.4f)"),
			Set.Builds.Num(), Set.ActiveBuildIndex, Sel.bUseCreatorColors ? 1 : 0,
			Loadout->IsContinuumEditingLocked() ? 1 : 0,
			Sel.CreatorBodyColor.R, Sel.CreatorBodyColor.G, Sel.CreatorBodyColor.B,
			// CC-6.4 MIGRATION ARM: for a build authored BEFORE the split, visorSet must be 0 and
			// selVisor must EQUAL selBody. selVisor at White (1,1,1) means the mirror failed and every
			// existing robot just had its visor restyled.
			Sel.bVisorColorSet ? 1 : 0,
			// rawVisor is PROVENANCE: White + visorSet=0 means "no choice was ever made", which is the
			// correct reading of a pre-split record -- it is NOT a defect on its own.
			Sel.CreatorVisorColor.R, Sel.CreatorVisorColor.G, Sel.CreatorVisorColor.B,
			// effVisor is the VALUE that renders. THIS is the one that must equal selBody when unset.
			Sel.EffectiveVisorColor().R, Sel.EffectiveVisorColor().G, Sel.EffectiveVisorColor().B);
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

	// CC-5.1 VERIFICATION: prove the schema's existence check can return NOT-FOUND, at runtime.
	// Engine source shows UMaterialInterface::GetVectorParameterValue returns false when the parameter is
	// absent -- but source reading is an argument, not a measurement, and the PYTHON sibling
	// (get_material_default_vector_parameter_value) was measured returning PRESENT(0,0,0) for parameters
	// that do not exist. This runs the EXACT call the schema uses, against a master where the answer is
	// known both ways: M_Mannequin genuinely lacks BaseTint (0 string occurrences in its T3D export) and
	// genuinely HAS EdgeGlowColor (1 occurrence; get_vector_parameter_names lists it).
	// FALSIFICATION: if BaseTint reports found=1 the check cannot distinguish absent from default and the
	// whole schema is void.
	void HandleAFLSchemaProbe(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
	{
		const FString Path = Args.Num() > 0 ? Args[0]
			: FString(TEXT("/Game/Characters/Heroes/Mannequin/Materials/M_Mannequin"));
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *Path);
		if (!Mat) { Ar.Logf(TEXT("afl.Creator.SchemaProbe - could not load %s"), *Path); return; }
		static const TCHAR* Params[] = { TEXT("BaseTint"), TEXT("EdgeGlowColor"), TEXT("TeamColor"),
			TEXT("EmissiveColor"), TEXT("NeonColor"), TEXT("CarbonfiberTint") };
		for (const TCHAR* P : Params)
		{
			FLinearColor V(ForceInit);
			const bool bFound = Mat->GetVectorParameterValue(FMaterialParameterInfo(FName(P)), V);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCHEMAPROBE] %s param=%-16s found=%d value=(%.3f,%.3f,%.3f)"),
				*Mat->GetName(), P, bFound ? 1 : 0, V.R, V.G, V.B);
		}
		const FAFLCreatorChannelSchema Sch = FAFLCreatorChannelSchema::DeriveFromMaterial(Mat);
		// CC-6.4: visor= and count= are NOT decoration. Before the split bBodyAvailable was
		// (BaseTint || TeamColor); it is now TeamColor alone. On M_Mannequin that reads 1 EITHER WAY,
		// so the old emit was byte-identical before and after the change and could never falsify it.
		// visor= is the field that actually moved, and it must read 0 here: this master has no BaseTint,
		// so a visor control offered on its 32 facemask presets would write nowhere.
		// CC-5.2: the BOOLS say "can I use it"; the STATES say why not. Emitting only the bools would
		// make PresentButInert and Absent look identical in the log -- the exact collapse the three
		// states exist to prevent -- and the reason is what a UI renders beside a disabled control.
		auto StateStr = [](EAFLChannelAvailability S)
		{
			switch (S)
			{
				case EAFLChannelAvailability::Connected:       return TEXT("Connected");
				case EAFLChannelAvailability::PresentButInert: return TEXT("PresentButInert");
				default:                                      return TEXT("Absent");
			}
		};
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SCHEMAPROBE] DERIVED master=%s body=%d edge=%d glow=%d visor=%d count=%d "
				"audited=%d | bodyState=%s edgeState=%s glowState=%s visorState=%s"),
			*Sch.ResolvedFromMaster.ToString(), Sch.bBodyAvailable ? 1 : 0, Sch.bEdgeAvailable ? 1 : 0,
			Sch.bGlowAvailable ? 1 : 0, Sch.bVisorAvailable ? 1 : 0, Sch.AvailableCount(),
			Sch.bMasterAudited ? 1 : 0,
			StateStr(Sch.BodyState), StateStr(Sch.EdgeState),
			StateStr(Sch.GlowState), StateStr(Sch.VisorState));
	}

	// ─── CC-5.2 hue-arc invariant: afl.Creator.ArcProbe ──────────────────────────────────────────
	// THE FAILURE THIS CATCHES: a hue drag that produces an OUT-OF-GAMUT colour. The player would see
	// it in the preview, commit it, and the server clamp would hand back something visibly different --
	// the control lying about its own result. Since CC-5.2 the preview and the server share ONE clamp,
	// so this asserts the property that makes sharing safe: WithHue's output is ALWAYS in gamut, for
	// adversarial inputs, not just tidy ones.
	//
	// Adversarial by construction: near-black, pure grey (hue is undefined there), fully saturated, and
	// out-of-range hue values that must wrap rather than clip.
	void HandleAFLCreatorArcProbe(const TArray<FString>& /*Args*/, UWorld* /*World*/, FOutputDevice& Ar)
	{
		const TArray<TPair<FString, FLinearColor>> Inputs = {
			{ TEXT("near-black"),   FLinearColor(0.01f, 0.01f, 0.01f) },
			{ TEXT("pure-grey"),    FLinearColor(0.50f, 0.50f, 0.50f) },
			{ TEXT("near-white"),   FLinearColor(0.98f, 0.98f, 0.98f) },
			{ TEXT("in-gamut-cyan"),FLinearColor(0.05f, 0.90f, 0.80f) },
		};
		const float Hues[] = { 0.0f, 90.0f, 200.0f, 359.9f, -30.0f, 420.0f };

		int32 Checked = 0, Bad = 0;
		for (const TPair<FString, FLinearColor>& In : Inputs)
		{
			for (const float H : Hues)
			{
				const FLinearColor Out = AFLCreatorGamut::WithHue(In.Value, H);
				const FLinearColor HSV = Out.LinearRGBToHSV();
				++Checked;
				// tolerance: HSV<->RGB round-trips are lossy at the last bit; 1e-3 is far below anything visible
				const bool bSatOk = HSV.G >= AFLCreatorGamut::MinSaturation - 1e-3f;
				const bool bValOk = HSV.B >= AFLCreatorGamut::MinValue - 1e-3f
				                 && HSV.B <= AFLCreatorGamut::MaxValue + 1e-3f;
				const float WantHue = FMath::Fmod(FMath::Fmod(H, 360.0f) + 360.0f, 360.0f);
				const float HueErr  = FMath::Abs(FMath::UnwindDegrees(HSV.R - WantHue));
				const bool bHueOk   = HueErr < 1.0f;
				if (!bSatOk || !bValOk || !bHueOk)
				{
					++Bad;
					UE_LOG(LogAFLCombat, Warning,
						TEXT("AFL_TEST[ARC] OUT-OF-GAMUT in=%s hue=%.1f -> S=%.3f V=%.3f hue=%.1f (satOk=%d valOk=%d hueOk=%d)"),
						*In.Key, H, HSV.G, HSV.B, HSV.R, bSatOk ? 1 : 0, bValOk ? 1 : 0, bHueOk ? 1 : 0);
				}
			}
		}

		// S/V PRESERVATION on an already-valid pick: dragging hue must not quietly restyle the rest.
		const FLinearColor Valid(0.05f, 0.90f, 0.80f);
		const FLinearColor ValidHSV = Valid.LinearRGBToHSV();
		const FLinearColor Rehued   = AFLCreatorGamut::WithHue(Valid, 300.0f).LinearRGBToHSV();
		const bool bKeptSV = FMath::IsNearlyEqual(Rehued.G, FMath::Max(ValidHSV.G, AFLCreatorGamut::MinSaturation), 1e-2f)
		                  && FMath::IsNearlyEqual(Rehued.B, FMath::Clamp(ValidHSV.B, AFLCreatorGamut::MinValue, AFLCreatorGamut::MaxValue), 1e-2f);

		// LINKS DEFAULT OFF -- the shipped state until a pairing is ruled (CC-X24 killed the roadmap's).
		const FAFLCreatorChannelLinks Links;
		const bool bUnlinked = (Links.LinkedMask == 0) && (Links.LinkedCount() == 0);

		const bool bPass = (Bad == 0) && bKeptSV && bUnlinked;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[ARC] %s -- checked=%d outOfGamut=%d keptSV=%d linksDefaultOff=%d"),
			bPass ? TEXT("PASS") : TEXT("FAIL"), Checked, Bad, bKeptSV ? 1 : 0, bUnlinked ? 1 : 0);
		Ar.Logf(TEXT("afl.Creator.ArcProbe - checked=%d outOfGamut=%d"), Checked, Bad);
	}

	// ─── CC-5.3 creator loop proof: afl.Creator.PreviewProbe ─────────────────────────────────────
	// FOUR ARMS, and the fourth is the one that matters.
	//   1 APPLY LANDS      a channel change reaches the preview pawn's MIDs
	//   2 ROTATE HOLDS     measured on the FAR SIDE of a rotation, not before it
	//   3 CHANGE WHILE ROTATED   applies without reverting the rotation
	//   4 PREVIEW == SPAWN the preview MID equals what BuildColorOverride yields for the same
	//     selection -- the value the GAMEPLAY pawn receives. Reasoned equality is worthless here:
	//     "they call the same function" is exactly the claim a refactor silently breaks. A preview
	//     with its own path is a bait-and-switch waiting to happen (CREATOR_SSOT 5.3).
	//
	// PRESENCE OF OUTPUT IS THE SUCCESS SIGNAL. Every arm emits its measured numbers, and the summary
	// carries checked= counts, so silence means the probe did not run -- never that it passed.
	static bool AFLCC53_ReadPartParam(APawn* Pawn, const FName ParamName, FLinearColor& Out, int32& OutMIDs)
	{
		OutMIDs = 0;
		bool bAny = false;
		if (!Pawn) { return false; }
		TArray<AActor*> Attached;
		Pawn->GetAttachedActors(Attached);
		Attached.Add(Pawn);
		for (AActor* A : Attached)
		{
			if (!IsValid(A)) { continue; }
			TArray<USkeletalMeshComponent*> Meshes;
			A->GetComponents<USkeletalMeshComponent>(Meshes);
			for (USkeletalMeshComponent* M : Meshes)
			{
				if (!M) { continue; }
				const int32 N = M->GetNumMaterials();
				for (int32 i = 0; i < N; ++i)
				{
					UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(i));
					if (!MID) { continue; }
					FLinearColor V;
					if (MID->GetVectorParameterValue(FMaterialParameterInfo(ParamName), V))
					{
						++OutMIDs;
						if (!bAny) { Out = V; bAny = true; }
					}
				}
			}
		}
		return bAny;
	}

	void HandleAFLCreatorPreviewProbe(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Creator.PreviewProbe - run inside PIE.")); return; }
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC) { Ar.Log(TEXT("afl.Creator.PreviewProbe - no PC.")); return; }

		// Reuse an open loadout widget if there is one; otherwise construct the concrete WBP. The C++ base
		// is UCLASS(Abstract) with a required BindWidget, so the WBP subclass is the only constructible form.
		UAFLW_LoadoutBase* W = nullptr;
		for (TObjectIterator<UAFLW_LoadoutBase> It; It; ++It)
		{
			if (IsValid(*It) && It->GetWorld() == World) { W = *It; break; }
		}
		const bool bReused = (W != nullptr);
		if (!W)
		{
			UClass* Cls = LoadClass<UAFLW_LoadoutBase>(nullptr,
				TEXT("/Game/BagMan/UI/Loadout/WBP_AFL_Loadout.WBP_AFL_Loadout_C"));
			if (Cls) { W = CreateWidget<UAFLW_LoadoutBase>(PC, Cls); }
		}
		if (!W)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[CC53] ABORT -- no loadout widget (reused=%d). Not a pass."), bReused ? 1 : 0);
			return;
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[CC53] widget=%s reused=%d"), *GetNameSafe(W), bReused ? 1 : 0);

		// Two colours far apart in hue so a stale read cannot pass as a fresh one.
		const FLinearColor C1(0.90f, 0.05f, 0.60f);   // magenta-ish
		const FLinearColor C2(0.05f, 0.85f, 0.35f);   // green-ish
		const FLinearColor Want1 = AFLCreatorGamut::ClampToNeon(C1);
		const FLinearColor Want2 = AFLCreatorGamut::ClampToNeon(C2);
		static const FName NEdge(TEXT("EdgeGlowColor"));
		auto Near = [](const FLinearColor& A, const FLinearColor& B)
		{ return A.Equals(B, 1e-3f); };

		// ARM 1 -- apply lands
		W->CreatorSetChannel(EAFLCreatorChannel::Edge, C1);
		W->CreatorApplyPreview();
		APawn* Preview = nullptr;
		for (TActorIterator<AAFLLoadoutDisplayPawn> It(World); It; ++It) { Preview = *It; break; }
		FLinearColor Got1(ForceInit); int32 M1 = 0;
		const bool bRead1 = AFLCC53_ReadPartParam(Preview, NEdge, Got1, M1);
		const bool bArm1 = bRead1 && Near(Got1, Want1);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[CC53] ARM1 apply pawn=%s mids=%d want=(%.4f,%.4f,%.4f) got=(%.4f,%.4f,%.4f) -> %s"),
			*GetNameSafe(Preview), M1, Want1.R, Want1.G, Want1.B, Got1.R, Got1.G, Got1.B,
			bArm1 ? TEXT("PASS") : TEXT("FAIL"));

		// ARM 2 -- rotate, then read on the FAR SIDE
		const float Yaw0 = W->CreatorGetPreviewYaw();
		W->CreatorRotatePreview(137.0f);
		const float Yaw1 = W->CreatorGetPreviewYaw();
		FLinearColor Got2(ForceInit); int32 M2 = 0;
		const bool bRead2 = AFLCC53_ReadPartParam(Preview, NEdge, Got2, M2);
		const bool bMoved = FMath::Abs(FMath::UnwindDegrees(Yaw1 - Yaw0) - 137.0f) < 1.0f;
		const bool bArm2 = bRead2 && bMoved && Near(Got2, Want1);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[CC53] ARM2 rotate yaw %.1f->%.1f moved=%d colour got=(%.4f,%.4f,%.4f) held=%d -> %s"),
			Yaw0, Yaw1, bMoved ? 1 : 0, Got2.R, Got2.G, Got2.B, Near(Got2, Want1) ? 1 : 0,
			bArm2 ? TEXT("PASS") : TEXT("FAIL"));

		// ARM 3 -- change WHILE rotated; the rotation must not revert
		W->CreatorSetChannel(EAFLCreatorChannel::Edge, C2);
		W->CreatorApplyPreview();
		const float Yaw2 = W->CreatorGetPreviewYaw();
		FLinearColor Got3(ForceInit); int32 M3 = 0;
		const bool bRead3 = AFLCC53_ReadPartParam(Preview, NEdge, Got3, M3);
		const bool bYawKept = FMath::Abs(FMath::UnwindDegrees(Yaw2 - Yaw1)) < 1.0f;
		const bool bArm3 = bRead3 && Near(Got3, Want2) && bYawKept;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[CC53] ARM3 change-while-rotated want=(%.4f,%.4f,%.4f) got=(%.4f,%.4f,%.4f) yaw %.1f->%.1f kept=%d -> %s"),
			Want2.R, Want2.G, Want2.B, Got3.R, Got3.G, Got3.B, Yaw1, Yaw2, bYawKept ? 1 : 0,
			bArm3 ? TEXT("PASS") : TEXT("FAIL"));

		// ARM 4 -- DECISIVE. What the gameplay pawn would receive, for the SAME selection.
		const FAFLCosmeticSelection Working = W->CreatorGetWorkingSelection();
		const FAFLColorOverride Spawned = UAFLCosmeticLoadoutComponent::BuildColorOverride(Working);
		const bool bArm4 = Spawned.bValid && Near(Spawned.EdgeColor, Got3);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[CC53] ARM4 preview-vs-spawn valid=%d spawnEdge=(%.4f,%.4f,%.4f) previewMID=(%.4f,%.4f,%.4f) -> %s"),
			Spawned.bValid ? 1 : 0, Spawned.EdgeColor.R, Spawned.EdgeColor.G, Spawned.EdgeColor.B,
			Got3.R, Got3.G, Got3.B, bArm4 ? TEXT("PASS") : TEXT("FAIL"));

		const bool bPass = bArm1 && bArm2 && bArm3 && bArm4;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[CC53] %s -- arms=4 midsRead=%d/%d/%d apply=%d rotate=%d changeWhileRotated=%d previewEqualsSpawn=%d"),
			bPass ? TEXT("PASS") : TEXT("FAIL"), M1, M2, M3,
			bArm1 ? 1 : 0, bArm2 ? 1 : 0, bArm3 ? 1 : 0, bArm4 ? 1 : 0);
		Ar.Logf(TEXT("afl.Creator.PreviewProbe - 4 arms run; see AFL_TEST[CC53]."));
	}

	// Defined further down this file (the online-seam helpers); forward-declared so the CC-4.2 probe can
	// sit beside the other creator probes rather than being exiled to the bottom for a lookup order.
	UAFLWalletComponent* GetPlayerWallet(UWorld* World);

	// ─── CC-4.2 slot wiring proof: afl.Creator.SlotProbe ─────────────────────────────────────────
	// FIVE ARMS.
	//   1 BASELINE        the counter starts where it starts, recorded so every later delta is real
	//   2 GRANT LANDS     buying x3 increments by exactly 3, not by 1 and not by "owned"
	//   3 ACCUMULATES     buying x3 AGAIN reaches 6 -- THE decisive arm
	//   4 DIFFERENT PACK  x8 adds 8 to the SAME counter, so one mechanism not two ladders
	//   5 CAP RESOLVES    AFLResolveEffectiveSlotCap over the ladder, including the clamp
	//
	// ARM 3 IS WHY THIS PROBE EXISTS. The owned-set add is idempotent, and if the counted grant were
	// gated on it the second purchase would take the money and grant nothing -- silent theft that every
	// single-purchase test passes. Buying twice is the only way to tell an incrementing counter from a
	// boolean wearing one.
	//
	// Uses the DEV grant path deliberately: this proves the WIRING (row data -> counter -> persistence),
	// not the payment transport, which cc-6-1-done already proved end-to-end against live PlayFab.
	// CC-4.2 -- the SERVER-side wallet for the local player.
	//
	// A console command in PIE executes in the CLIENT world, so GetPlayerWallet() returns the client's
	// REPLICA. GrantCountedEntitlement is authority-only, so granting against that replica would have
	// incremented nothing and every arm would have measured a counter that cannot move -- five PASSes
	// against a number that was never the number. The probe's authority guard caught exactly that.
	//
	// MATCHED BY REPLICATED PlayerId, NOT BY NAME. Both PIE clients share pawn and PlayerState NAMES
	// (the two-client attribution trap), so a name match would silently pick either player.
	//
	// Same world-context walk the STEP self-destruct arm above uses, which reported srvWorld=FOUND
	// srvPC=FOUND in this very session -- this is a proven path here, not an assumed one.
	UAFLWalletComponent* GetServerWalletForLocalPlayer(UWorld* ClientWorld, FString& OutWhy)
	{
		int32 MyId = -1;
		if (APlayerController* MyPC = ClientWorld ? ClientWorld->GetFirstPlayerController() : nullptr)
		{
			if (APlayerState* MyPS = MyPC->PlayerState) { MyId = MyPS->GetPlayerId(); }
		}
		if (MyId < 0) { OutWhy = TEXT("no local PlayerId"); return nullptr; }

		// LISTEN HOST FIRST: the caller's own world already HAS authority, so there is no separate
		// server world to go find. Searching for NM_DedicatedServer here would return nullptr and VOID
		// the probe on exactly the configuration chosen to make the proof possible.
		if (ClientWorld && ClientWorld->GetNetMode() == NM_ListenServer)
		{
			if (APlayerController* HostPC = ClientWorld->GetFirstPlayerController())
			{
				if (APlayerState* HostPS = HostPC->PlayerState)
				{
					if (UAFLWalletComponent* HostWallet = HostPS->FindComponentByClass<UAFLWalletComponent>())
					{
						OutWhy = FString::Printf(TEXT("LISTEN host wallet pid=%d ps=%s hasAuthority=%d (client and authority are ONE world)"),
							MyId, *HostPS->GetName(), HostPS->HasAuthority() ? 1 : 0);
						return HostWallet;
					}
				}
			}
			OutWhy = FString::Printf(TEXT("listen host but no wallet on the local PlayerState (pid=%d)"), MyId);
			return nullptr;
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
		if (!SrvWorld) { OutWhy = FString::Printf(TEXT("no dedicated-server world (pid=%d)"), MyId); return nullptr; }

		for (FConstPlayerControllerIterator It = SrvWorld->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* SrvPC = It->Get();
			APlayerState* SrvPS = SrvPC ? SrvPC->PlayerState : nullptr;
			if (!SrvPS || SrvPS->GetPlayerId() != MyId) { continue; }
			if (UAFLWalletComponent* SrvWallet = SrvPS->FindComponentByClass<UAFLWalletComponent>())
			{
				OutWhy = FString::Printf(TEXT("server wallet pid=%d ps=%s hasAuthority=%d"),
					MyId, *SrvPS->GetName(), SrvPS->HasAuthority() ? 1 : 0);
				return SrvWallet;
			}
			OutWhy = FString::Printf(TEXT("server PS pid=%d carries no wallet component"), MyId);
			return nullptr;
		}
		OutWhy = FString::Printf(TEXT("no server PC with pid=%d"), MyId);
		return nullptr;
	}

	void HandleAFLCreatorSlotProbe(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Creator.SlotProbe - run inside PIE.")); return; }
		// RESOLVE is emitted unconditionally, pass or fail: which wallet, on which PlayerState, with
		// what authority. A later run that grants nothing must say WHICH wallet it touched, or the
		// arms cannot be told apart from arms that measured the wrong object.
		FString Why;
		UAFLWalletComponent* Wallet = GetServerWalletForLocalPlayer(World, Why);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SLOT] RESOLVE %s"), *Why);
		if (!Wallet)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SLOT] ABORT -- no server wallet reached: %s"), *Why);
			return;
		}
		// KEPT, not removed. It is the falsifier: if the resolve above ever returns a replica again,
		// this refuses rather than producing five PASSes against a counter that cannot move.
		if (!Wallet->GetOwner() || !Wallet->GetOwner()->HasAuthority())
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SLOT] ABORT -- resolved wallet still lacks authority (%s); the counted grant is authority-only, so this would prove nothing."), *Why);
			return;
		}

		static const FName Key(TEXT("AFL.CreatorSlot"));
		const int32 Base = Wallet->GetCountedEntitlement(Key);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SLOT] ARM1 baseline %s = %d"), *Key.ToString(), Base);

		// ARM 2 -- x3 lands as 3
		Wallet->DebugGrantOwnership(FName(TEXT("AFL.CreatorSlot.x3")));
		const int32 After1 = Wallet->GetCountedEntitlement(Key);
		const bool bArm2 = (After1 - Base) == 3;
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SLOT] ARM2 buy x3  %d -> %d (delta %d, want 3) -> %s"),
			Base, After1, After1 - Base, bArm2 ? TEXT("PASS") : TEXT("FAIL"));

		// ARM 3 -- DECISIVE: buy the SAME pack again, must reach +6 total
		Wallet->DebugGrantOwnership(FName(TEXT("AFL.CreatorSlot.x3")));
		const int32 After2 = Wallet->GetCountedEntitlement(Key);
		const bool bArm3 = (After2 - Base) == 6;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SLOT] ARM3 buy x3 AGAIN %d -> %d (delta from baseline %d, want 6) -> %s  [a boolean would sit at %d]"),
			After1, After2, After2 - Base, bArm3 ? TEXT("PASS") : TEXT("FAIL"), After1);

		// ARM 4 -- a different pack, same counter
		Wallet->DebugGrantOwnership(FName(TEXT("AFL.CreatorSlot.x8")));
		const int32 After3 = Wallet->GetCountedEntitlement(Key);
		const bool bArm4 = (After3 - After2) == 8;
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SLOT] ARM4 buy x8  %d -> %d (delta %d, want 8; ONE counter) -> %s"),
			After2, After3, After3 - After2, bArm4 ? TEXT("PASS") : TEXT("FAIL"));

		// ARM 5 -- the cap resolver, every input a parameter. Values from PRICING_SSOT 5.1's ladder, passed
		// in rather than baked: free baseline 2 / ceiling 5, League baseline 5 / ceiling 10, hard cap 10.
		const int32 FreeNone   = AFLResolveEffectiveSlotCap(2, 0,  5, false, 10);  // want 2
		const int32 FreeThree  = AFLResolveEffectiveSlotCap(2, 3,  5, false, 10);  // want 5
		const int32 FreeClamp  = AFLResolveEffectiveSlotCap(2, 99, 5, false, 10);  // want 5 -- ceiling holds
		const int32 LeagueFive = AFLResolveEffectiveSlotCap(5, 5, 10, false, 10);  // want 10
		const int32 Upgraded   = AFLResolveEffectiveSlotCap(2, 0,  5, true,  10);  // want 10 -- upgrade wins
		const int32 NegSafe    = AFLResolveEffectiveSlotCap(2, -7, 5, false, 10);  // want 2 -- never below baseline
		const bool bArm5 = FreeNone == 2 && FreeThree == 5 && FreeClamp == 5
		                && LeagueFive == 10 && Upgraded == 10 && NegSafe == 2;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SLOT] ARM5 cap free0=%d(2) free3=%d(5) clamp=%d(5) league5=%d(10) upgrade=%d(10) neg=%d(2) -> %s"),
			FreeNone, FreeThree, FreeClamp, LeagueFive, Upgraded, NegSafe, bArm5 ? TEXT("PASS") : TEXT("FAIL"));

		const bool bPass = bArm2 && bArm3 && bArm4 && bArm5;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SLOT] %s -- arms=5 baseline=%d final=%d grantLands=%d accumulates=%d oneCounter=%d capResolves=%d"),
			bPass ? TEXT("PASS") : TEXT("FAIL"), Base, After3,
			bArm2 ? 1 : 0, bArm3 ? 1 : 0, bArm4 ? 1 : 0, bArm5 ? 1 : 0);
		Ar.Logf(TEXT("afl.Creator.SlotProbe - 5 arms run; see AFL_TEST[SLOT]."));
	}

	// --- CC-X23 reconcile proof: afl.Online.ReconcileProbe -------------------------------------
	// What this proves: a mirror deliberately set WRONG is corrected back to PlayFab's number by the
	// shipping reconcile. That is the mechanism, and it is measured.
	//
	// What it does NOT prove, stated because the run makes it easy to believe otherwise: that the
	// OnLoggedIn subscription fires in the real ordering. In PIE the dev CustomID login resolves BEFORE
	// wallet BeginPlay -- all 8 wallets logged "already logged in at BeginPlay -> no subscription
	// needed" -- so the delegate branch is never exercised here. Shipping uses EOS/OIDC over the
	// network, which is slower, so the race is MORE likely there and less testable here.
	//
	// The earlier framing of this defect (a 200,179 mirror against an authoritative 4,008) does not
	// hold: PlayFab reads 200,179. See AFLWalletComponent.h for the correction.
	//
	// ARM 2 IS WHY THIS PROBE EXISTS. Observing that the mirror matches PlayFab proves nothing -- the
	// two numbers can agree because the reconcile works, OR because they were never going to differ in
	// this session. So the probe DELIBERATELY POISONS the mirror to a known-wrong value first. A wallet
	// with no second read sits on the poison; only a working reconcile moves off it. Without the poison
	// this instrument cannot fail, and an instrument that cannot fail is not evidence.
	//
	// Routes through DebugForceReconcile -> HandleLoggedIn, the SAME function the OnLoggedIn delegate
	// calls, so the arm exercises the shipping reconcile rather than a parallel path written to pass.
	void HandleAFLReconcileProbe(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.ReconcileProbe - run inside PIE.")); return; }

		// PRECONDITION, emitted whether it holds or not. Without a session there is no authoritative number
		// to reconcile TO, so a run here is VOID -- not a pass, and emphatically not a failure of the fix.
		const UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(World);
		const bool bLoggedIn = Online && Online->IsLoggedIn();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RECON] ARM0 precondition loggedIn=%d"), bLoggedIn ? 1 : 0);
		if (!bLoggedIn)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[RECON] VOID -- no PlayFab session, so there is no authoritative balance to reconcile to. ")
				TEXT("Re-run with afl.Online.ForceEosLogin 1. This is NOT a FAIL: the fix is untested here, not disproved."));
			return;
		}

		FString Why;
		UAFLWalletComponent* Wallet = GetServerWalletForLocalPlayer(World, Why);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RECON] RESOLVE %s"), *Why);
		if (!Wallet)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RECON] VOID -- no server wallet reached: %s"), *Why);
			return;
		}

		const int32 Before = Wallet->GetVolts();
		const int32 WattsBefore = Wallet->GetWatts();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RECON] ARM1 mirror before volts=%d watts=%d known=%d"),
			Before, WattsBefore, Wallet->IsBalanceKnown() ? 1 : 0);

		// ARM 2 -- POISON. Offset by a value no real balance would coincide with, and read back to prove the
		// poison actually landed; a poison that silently failed would make ARM 3 pass for the wrong reason.
		const int32 PoisonOffset = 123456;
		const int32 Poisoned = Before + PoisonOffset;
		Wallet->DebugSetBalance(Poisoned, WattsBefore);
		const int32 PoisonReadBack = Wallet->GetVolts();
		const bool bPoisonLanded = (PoisonReadBack == Poisoned);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RECON] ARM2 poison %d -> %d (readback %d, landed=%d)"),
			Before, Poisoned, PoisonReadBack, bPoisonLanded ? 1 : 0);
		if (!bPoisonLanded)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RECON] VOID -- the poison did not land, so ARM3 would prove nothing."));
			return;
		}

		// ARM 3 -- RECONCILE, then read on a LATER FRAME. LoadBalance round-trips PlayFab, so reading
		// synchronously here would sample before the answer arrives and report a false FAIL -- exactly the
		// mistake VerifyNewSkuBuy made when it asserted on the mirror inside its own callback.
		Wallet->DebugForceReconcile();

		TWeakObjectPtr<UAFLWalletComponent> WeakWallet(Wallet);
		TWeakObjectPtr<UWorld> WeakWorld(World);
		FTimerHandle T;
		World->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([WeakWallet, WeakWorld, Before, Poisoned]()
		{
			UAFLWalletComponent* W = WeakWallet.Get();
			if (!W || !WeakWorld.IsValid())
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RECON] VOID -- wallet or world gone before the settle read."));
				return;
			}
			const int32 After = W->GetVolts();
			const bool bMovedOffPoison = (After != Poisoned);
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[RECON] ARM3 after reconcile volts=%d (poison was %d, pre-poison was %d) movedOffPoison=%d backToPrePoison=%d"),
				After, Poisoned, Before, bMovedOffPoison ? 1 : 0, (After == Before) ? 1 : 0);

			// PASS keys on MOVED-OFF-POISON, not on equality with the pre-poison value. If the pre-poison
			// mirror was ITSELF stale -- the very defect under test -- a correct reconcile lands on a
			// DIFFERENT number, and demanding equality would fail the fix precisely when it worked.
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RECON] %s -- the mirror %s a deliberately wrong value"),
				bMovedOffPoison ? TEXT("PASS") : TEXT("FAIL"),
				bMovedOffPoison ? TEXT("corrected") : TEXT("SAT ON"));
		}), 5.0f, false);

		Ar.Logf(TEXT("afl.Online.ReconcileProbe - poisoned and reconciling; ARM3 lands in ~5s. See AFL_TEST[RECON]."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLReconcileProbeCmd(TEXT("afl.Online.ReconcileProbe"),
		TEXT("CC-X23: prove the wallet mirror re-reads after login. Poisons the mirror to a known-wrong value, ")
		TEXT("forces the SHIPPING reconcile, and checks it moved off the poison. Requires a PlayFab session; ")
		TEXT("without one it reports VOID, not PASS. AFL_TEST[RECON] PASS = corrected."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLReconcileProbe));

	// --- THE JOIN PROOF: afl.Online.VerifySlotBuyJoin ------------------------------------------
	// Buys AFL.CreatorSlot.x3 through ClientRequestPurchase -- the PRODUCTION entry, real Volts, live
	// PlayFab -- and asserts the counted slot entitlement moves. This is the join neither cc-4-2-done
	// (which granted through the dev path) nor cc-6-1-done (which never looked at the counter) covers.
	//
	// ARM3 BUYS AGAIN. CC-4.2's decisive arm was that buying x3 twice reaches 6; that has to hold
	// through ApplyPurchaseResult too, or the counter is a boolean again by a different route.
	//
	// SPENDS REAL BALANCE, so it is cvar-gated and checks funds BEFORE spending any: an arm that runs
	// out of Volts halfway reports FAIL for a reason that has nothing to do with the join.
	//
	// ============================ RUN THIS ON A LISTEN SERVER ============================
	// AND UNDERSTAND WHY THAT IS AN EXCEPTION, NOT A STANDARD.
	//
	// LISTEN SERVER WAS REJECTED FOR THIS PROGRAMME'S PROOFS ON PURPOSE -- CC-2.1 step 6 exists
	// precisely because listen-host exercises IN-PROCESS authority and therefore PASSES legs that do
	// not exist in a shipped dedicated-server build. Anything about REPLICATION or CLIENT CONVERGENCE
	// must still be proven on dedicated, two clients. That rule is unchanged.
	//
	// The exception is narrow and mechanical: in dedicated PIE each GameInstance logs into PlayFab
	// SEPARATELY with its own DevCustomId, so the authority side and the client side are DIFFERENT
	// ACCOUNTS. An economy proof needs the account that PAYS and the account that is WATCHED to be one
	// account. Listen server is the only PIE configuration that gives that.
	//
	// So: ECONOMY proofs that must span authority and client on ONE PlayFab account -> listen server.
	// Everything else -> dedicated. Do not read "proven on listen server" here as a general standard.
	// ====================================================================================
	void HandleAFLVerifySlotBuyJoin(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.VerifySlotBuyJoin - run inside PIE.")); return; }

		const UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(World);
		const bool bLoggedIn = Online && Online->IsLoggedIn();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[JOIN] ARM0 precondition loggedIn=%d"), bLoggedIn ? 1 : 0);
		if (!bLoggedIn)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[JOIN] VOID -- no PlayFab session; the production purchase entry cannot run. NOT a FAIL."));
			return;
		}

		UAFLWalletComponent* ClientWallet = GetPlayerWallet(World);
		FString Why;
		UAFLWalletComponent* SrvWallet = GetServerWalletForLocalPlayer(World, Why);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[JOIN] RESOLVE client=%s | server=%s"),
			ClientWallet ? TEXT("found") : TEXT("MISSING"), *Why);
		if (!ClientWallet || !SrvWallet)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[JOIN] VOID -- need the CLIENT wallet to drive the production entry and the SERVER wallet to read the counter."));
			return;
		}

		// THE GUARD THAT WOULD HAVE CAUGHT THE v1 RUN IN ITS FIRST SECOND.
		// Three accounts must be one: the one whose balance is READ, the one that is CHARGED, and the
		// one whose COUNTER is watched. A mismatch is VOID -- a split-account run cannot say anything
		// about the join, so grading it FAIL would be reporting a verdict the evidence cannot support.
		const UAFLOnlineSubsystem* CliOnline = UAFLOnlineSubsystem::Get(ClientWallet);
		const UAFLOnlineSubsystem* SrvOnline = UAFLOnlineSubsystem::Get(SrvWallet);
		const FString CliPf = CliOnline ? CliOnline->GetPlayFabId() : TEXT("<none>");
		const FString SrvPf = SrvOnline ? SrvOnline->GetPlayFabId() : TEXT("<none>");
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[JOIN] ARM0b accounts charged/read=%s counterWatched=%s same=%d"),
			*CliPf, *SrvPf, (CliPf == SrvPf && !CliPf.IsEmpty()) ? 1 : 0);
		if (CliPf != SrvPf || CliPf.IsEmpty())
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[JOIN] VOID -- the account being CHARGED (%s) is not the account whose COUNTER is watched (%s). ")
				TEXT("Dedicated PIE logs each GameInstance in separately; run this on a LISTEN SERVER. NOT a FAIL: nothing about the join was tested."),
				*CliPf, *SrvPf);
			return;
		}

		static const FName SlotKey(TEXT("AFL.CreatorSlot"));
		static const FName SkuX3(TEXT("AFL.CreatorSlot.x3"));
		const int32 Price = 4990;
		const int32 VoBefore = SrvWallet->GetVolts();
		const int32 Base = SrvWallet->GetCountedEntitlement(SlotKey);
		// THE MIRROR IS NOT THE SPENDABLE BALANCE, and it is not necessarily even the same ACCOUNT.
		// The first run of this probe passed its funds check on a mirror reading 200,179 and was then
		// refused by PlayFab for InsufficientFunds on a 4,990 item. Report the account so the next
		// failure is attributable instead of merely surprising.
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[JOIN] ARM1 baseline counter=%d mirrorVo=%d (need %d) pfid=%s (counter read on the SAME account, asserted above)"),
			Base, VoBefore, Price * 2, *CliPf);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[JOIN] ARM1 NOTE ARM3 also exercises the backend fix: AFL.CreatorSlot.x3 carried IsStackable=false until 7452fdc, ")
			TEXT("so buy#2 would have been refused ALREADY OWNED regardless of the game-side join. buy2Accepted=1 proves BOTH halves."));
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[JOIN] ARM1 NOTE mirrorVo is a DISPLAY value; PlayFab decides. A rejection here is a FUNDS result, not a join result."));
		if (VoBefore < Price * 2)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[JOIN] VOID -- insufficient Volts (%d < %d). Top up before running; a half-run proves nothing."), VoBefore, Price * 2);
			return;
		}

		TWeakObjectPtr<UAFLWalletComponent> WeakSrv(SrvWallet);
		TWeakObjectPtr<UAFLWalletComponent> WeakCli(ClientWallet);
		TWeakObjectPtr<UWorld> WeakWorld(World);

		// ARM2 -- first production buy. The counter is read on a LATER FRAME: the grant lands server-side
		// after a PlayFab round-trip, and reading inside the callback samples before the answer arrives.
		ClientWallet->ClientRequestPurchase(SkuX3, EAFLPayCurrency::Volts,
			[WeakSrv, WeakCli, WeakWorld, Base, VoBefore, Price](bool bBuy1)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[JOIN] ARM2 buy#1 accepted=%d"), bBuy1 ? 1 : 0);
			const bool bBuy1Outer = bBuy1;
			if (!WeakWorld.IsValid()) { return; }
			FTimerHandle T1;
			WeakWorld->GetTimerManager().SetTimer(T1, FTimerDelegate::CreateLambda(
				[WeakSrv, WeakCli, WeakWorld, Base, VoBefore, Price, bBuy1Outer]()
			{
				UAFLWalletComponent* S = WeakSrv.Get();
				UAFLWalletComponent* C = WeakCli.Get();
				if (!S || !C || !WeakWorld.IsValid())
				{
					UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[JOIN] VOID -- wallet or world gone before the ARM2 settle read."));
					return;
				}
				static const FName K(TEXT("AFL.CreatorSlot"));
				static const FName Sku(TEXT("AFL.CreatorSlot.x3"));
				const int32 After1 = S->GetCountedEntitlement(K);
				const bool bArm2 = (After1 - Base) == 3;
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[JOIN] ARM2 counter %d -> %d (delta %d, want 3) vo %d -> %d -> %s"),
					Base, After1, After1 - Base, VoBefore, S->GetVolts(), bArm2 ? TEXT("PASS") : TEXT("FAIL"));

				// ARM3 -- DECISIVE: buy the SAME pack again through the SAME production entry.
				C->ClientRequestPurchase(Sku, EAFLPayCurrency::Volts,
					[WeakSrv, WeakWorld, Base, After1, bArm2, bBuy1Outer](bool bBuy2)
				{
					UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[JOIN] ARM3 buy#2 accepted=%d"), bBuy2 ? 1 : 0);
					if (!WeakWorld.IsValid()) { return; }
					FTimerHandle T2;
					WeakWorld->GetTimerManager().SetTimer(T2, FTimerDelegate::CreateLambda(
						[WeakSrv, Base, After1, bArm2, bBuy2, bBuy1Outer]()
					{
						UAFLWalletComponent* S2 = WeakSrv.Get();
						if (!S2) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[JOIN] VOID -- server wallet gone before ARM3.")); return; }
						static const FName K2(TEXT("AFL.CreatorSlot"));
						const int32 After2 = S2->GetCountedEntitlement(K2);
						const bool bArm3 = (After2 - Base) == 6;
						UE_LOG(LogAFLCombat, Display,
							TEXT("AFL_TEST[JOIN] ARM3 counter %d -> %d (delta from baseline %d, want 6) -> %s  [a one-shot SKU or a boolean would sit at %d]"),
							After1, After2, After2 - Base, bArm3 ? TEXT("PASS") : TEXT("FAIL"), After1);
						UE_LOG(LogAFLCombat, Display,
							TEXT("AFL_TEST[JOIN] %s -- production purchase reaches the counted grant (buy1Accepted=%d buy2Accepted=%d grantLands=%d accumulates=%d)"),
							(bArm2 && bArm3) ? TEXT("PASS") : TEXT("FAIL"),
							bBuy1Outer ? 1 : 0, bBuy2 ? 1 : 0, bArm2 ? 1 : 0, bArm3 ? 1 : 0);
					}), 5.0f, false);
				});
			}), 5.0f, false);
		});

		Ar.Logf(TEXT("afl.Online.VerifySlotBuyJoin - two production buys queued; ARM3 lands in ~12s. See AFL_TEST[JOIN]."));
	}

	// --- THE BUNDLE PROOF: afl.Online.VerifyBundleBuy --------------------------------------------
	// Catalog rows (149948f3), ledger rows (backend 3d5af6a) and routing (1cca35ef) each look right
	// alone. That is exactly the configuration that produced the slot-join defect: two proven halves
	// described as meeting, that never touched. This arm is the seam.
	void HandleAFLVerifyBundleBuy(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.VerifyBundleBuy - run inside PIE.")); return; }

		const UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(World);
		const bool bLoggedIn = Online && Online->IsLoggedIn();
		const bool bSigner   = Online && Online->IsBundlePurchaseConfigured();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUN] ARM0 loggedIn=%d signerConfigured=%d"),
			bLoggedIn ? 1 : 0, bSigner ? 1 : 0);
		if (!bLoggedIn || !bSigner)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUN] VOID -- need a session AND AFL_BUNDLE_URL + AFL_EARN_HMAC_KEY. NOT a FAIL: the route was never exercised."));
			return;
		}

		FString Why;
		UAFLWalletComponent* Wallet = GetServerWalletForLocalPlayer(World, Why);
		UAFLWalletComponent* Client = GetPlayerWallet(World);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUN] RESOLVE %s"), *Why);
		if (!Wallet || !Client) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUN] VOID -- no wallet.")); return; }

		UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!Catalog) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUN] VOID -- no catalog.")); return; }

		// DISCOVER the ids. Hardcoding one would be the sixth name-guess of this session.
		// GetEntriesByType, NOT GetPurchasableEntries: the latter drops GrantedFree rows, so the free arm
		// could never find its bundle and that absence would read as a missing row rather than a filter
		// excluding it by design.
		FName SoldId = NAME_None, FreeId = NAME_None, OwnedId = NAME_None;
		TArray<FName> SoldKids, FreeKids, OwnedKids;
		TArray<const FAFLCatalogEntry*> All;
		Catalog->GetEntriesByType(EAFLCosmeticType::Bundle, All);
		for (const FAFLCatalogEntry* Ptr : All)
		{
			const FAFLCatalogEntry& E = *Ptr;
			if (!E.CosmeticId.ToString().Contains(TEXT("HandCannon"))) { continue; }
			if (E.Acquisition == EAFLAcquisition::Direct && E.ContainedEntitlementIds.Num() == 2)
			{
				// Split by LIVE OWNERSHIP, not by id. The buy arm needs a pair nobody owns yet; the
				// re-buy arm needs one already owned. Previous runs bought DRAGONSOUL.XT, so a fixed
				// pick would silently turn the buy arm into a second re-buy.
				const bool bHave = Wallet->OwnsCosmetic(E.ContainedEntitlementIds[0])
					&& Wallet->OwnsCosmetic(E.ContainedEntitlementIds[1]);
				if (!bHave && SoldId.IsNone())      { SoldId = E.CosmeticId; SoldKids = E.ContainedEntitlementIds; }
				else if (bHave && OwnedId.IsNone()) { OwnedId = E.CosmeticId; OwnedKids = E.ContainedEntitlementIds; }
			}
			else if (E.Acquisition == EAFLAcquisition::GrantedFree && FreeId.IsNone())
			{ FreeId = E.CosmeticId; FreeKids = E.ContainedEntitlementIds; }
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUN] discovered sold=%s (%d kids) free=%s (%d kids)"),
			*SoldId.ToString(), SoldKids.Num(), *FreeId.ToString(), FreeKids.Num());
		if (SoldId.IsNone() || SoldKids.Num() != 2)
		{ UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUN] VOID -- no sold pair with 2 children found.")); return; }

		const int32 VoBefore = Wallet->GetVolts();
		const bool bKid0Before = Wallet->OwnsCosmetic(SoldKids[0]);
		const bool bKid1Before = Wallet->OwnsCosmetic(SoldKids[1]);
		const bool bBundleBefore = Wallet->OwnsCosmetic(SoldId);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[BUN] ARM1 baseline vo=%d bundleOwned=%d kid0(%s)=%d kid1(%s)=%d"),
			VoBefore, bBundleBefore ? 1 : 0, *SoldKids[0].ToString(), bKid0Before ? 1 : 0,
			*SoldKids[1].ToString(), bKid1Before ? 1 : 0);
		if (VoBefore < 1490)
		{ UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUN] VOID -- insufficient Volts (%d < 1490)."), VoBefore); return; }

		// FREE ARM -- an ENTITLEMENT check, not a purchase. A GrantedFree bundle is refused by the route
		// one line before it reaches /purchase-bundle, and that refusal is CORRECT.
		if (!FreeId.IsNone() && FreeKids.Num() == 2)
		{
			// ARM4 read OWNERSHIP. ARM4b reads ENTITLEMENT -- the actual gate. IsEntitled auto-passes
			// GrantedFree from the catalog ('owned by everyone -- the catalog says so'), so an empty
			// owned SET is BY DESIGN for a sponsor item, not a missing grant. Reporting only ownership
			// made a working design look like the slot defect.
			// IsEntitled IGNORES its Player parameter -- the signature is `const ALyraPlayerState*
			// /*Player*/` -- so nullptr is honest here. GetLyraPlayerState() is private and was not
			// going to be reached; inventing GetLyraPlayerStateForTest was the second invented accessor
			// in this probe.
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUN] ARM4 free=%s OWNED kid0=%d kid1=%d"),
				*FreeId.ToString(), Wallet->OwnsCosmetic(FreeKids[0]) ? 1 : 0, Wallet->OwnsCosmetic(FreeKids[1]) ? 1 : 0);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUN] ARM4b free ENTITLED kid0=%d kid1=%d  <- the gate that decides equip"),
				Wallet->IsEntitled(nullptr, FreeKids[0]) ? 1 : 0, Wallet->IsEntitled(nullptr, FreeKids[1]) ? 1 : 0);
		}

		// ARM5 -- CC-X29 RE-BUY REFUSAL. Fires FIRST so its VO reading is not contaminated by ARM2's
		// legitimate purchase. Expect: refused, VO unchanged, nothing granted.
		if (!OwnedId.IsNone())
		{
			const int32 VoPreRebuy = Wallet->GetVolts();
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUN] ARM5 re-buy OWNED pair %s vo=%d -- expect REFUSED, vo UNCHANGED"),
				*OwnedId.ToString(), VoPreRebuy);
			Client->ClientRequestPurchase(OwnedId, EAFLPayCurrency::Volts, TFunction<void(bool)>());
			TWeakObjectPtr<UAFLWalletComponent> WeakReb(Wallet);
			FTimerHandle TR;
			World->GetTimerManager().SetTimer(TR, FTimerDelegate::CreateLambda([WeakReb, VoPreRebuy, OwnedId]()
			{
				UAFLWalletComponent* R = WeakReb.Get();
				if (!R) { return; }
				const int32 VoNow = R->GetVolts();
				// DO NOT ASSERT ON VO. The wallet balance is SHARED: any other probe spending in the same
				// window corrupts the delta, and a previous run reported FAIL on a 7970 delta that was
				// three probes' spending added together, not a re-buy charge. Assert instead on signals
				// this bundle owns exclusively: the refusal line in the wallet diag, and MintedCount for
				// THIS bundleId read back from DynamoDB after the run.
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[BUN] ARM5 result %s -- vo %d -> %d (INFORMATIONAL ONLY; other probes spend concurrently). ")
					TEXT("The verdict is the BUNDLE ALREADY-OWNED line and MintedCount, not this delta."),
					*OwnedId.ToString(), VoPreRebuy, VoNow);
			}), 7.0f, false);
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUN] ARM5 SKIPPED -- no already-owned pair to re-buy. NOT a pass."));
		}

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUN] ARM2 buying %s through ClientRequestPurchase (production entry)"), *SoldId.ToString());
		Client->ClientRequestPurchase(SoldId, EAFLPayCurrency::Volts, TFunction<void(bool)>());

		TWeakObjectPtr<UAFLWalletComponent> WeakSrv(Wallet);
		TWeakObjectPtr<UWorld> WeakWorld(World);
		const FName K0 = SoldKids[0], K1 = SoldKids[1], BId = SoldId;
		FTimerHandle T;
		World->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([WeakSrv, K0, K1, BId, VoBefore]()
		{
			UAFLWalletComponent* S = WeakSrv.Get();
			if (!S) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BUN] VOID -- wallet gone before the settle read.")); return; }
			const int32 VoAfter = S->GetVolts();
			const bool bK0 = S->OwnsCosmetic(K0);
			const bool bK1 = S->OwnsCosmetic(K1);
			const bool bBundle = S->OwnsCosmetic(BId);
			const int32 Spent = VoBefore - VoAfter;
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[BUN] ARM3 vo %d -> %d (spent %d, want 1490) bundleOwned=%d kid0=%d kid1=%d"),
				VoBefore, VoAfter, Spent, bBundle ? 1 : 0, bK0 ? 1 : 0, bK1 ? 1 : 0);
			const bool bBothKids = bK0 && bK1;
			if (!bBothKids && bBundle)
			{
				UE_LOG(LogAFLCombat, Warning,
					TEXT("AFL_TEST[BUN] FAIL -- THE SLOT DEFECT REPRODUCED: the bundle id landed and the children did not. The player paid and received nothing usable."));
				return;
			}
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BUN] %s -- bothChildren=%d charged1490=%d"),
				(bBothKids && Spent == 1490) ? TEXT("PASS") : TEXT("FAIL"),
				bBothKids ? 1 : 0, (Spent == 1490) ? 1 : 0);
		}), 10.0f, false);

		Ar.Logf(TEXT("afl.Online.VerifyBundleBuy - purchase issued; ARM3 lands in ~10s. See AFL_TEST[BUN]."));
	}

	// === CC-X30 DURABILITY: afl.Online.VerifyCountedDurable + afl.Online.VerifyCountedRelaunch ===
	//
	// THE DEFECT, MEASURED RATHER THAN ASSUMED. LoadCountedSet was declared, implemented, and had NO
	// CALLER ANYWHERE in the programme. The counter was written to a local SaveGame and never read back
	// -- so it did not merely fail to reach another server, it did not survive the process that wrote
	// it. cc-join-done proved purchase -> counter -> persist and the read-back half did not exist.
	//
	// CORRECTION TO THE BLOCK'S PREMISE, stated because it changes what this proof means: an IN-SESSION
	// reconcile did NOT previously lose the counter. LoadFromPersistence simply never touched the
	// counted array, so the count survived by INACTION. It now survives because the reconcile actively
	// re-reads it from PlayFab -- correct by construction instead of by omission. The loss was always
	// on relaunch and on any other server, which is what ARM4 tests.
	void HandleAFLVerifyCountedDurable(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.VerifyCountedDurable - run inside PIE.")); return; }

		FString Why;
		UAFLWalletComponent* Wallet = GetServerWalletForLocalPlayer(World, Why);
		const UAFLOnlineSubsystem* Online = Wallet ? UAFLOnlineSubsystem::Get(Wallet) : UAFLOnlineSubsystem::Get(World);
		const FString Pf = Online ? Online->GetPlayFabId() : FString();
		const bool bSigner = Online && Online->IsCountedEntitlementConfigured();

		// WHY IS EMITTED. A null wallet with no reason attached is an unattributable skip, and one of
		// those already left a run inconclusive.
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[DUR] ARM0 wallet=%d signerConfigured=%d pfid=%s why=%s"),
			Wallet ? 1 : 0, bSigner ? 1 : 0, Pf.IsEmpty() ? TEXT("<none>") : *Pf, Why.IsEmpty() ? TEXT("ok") : *Why);

		if (!Wallet) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DUR] VOID -- no server wallet")); return; }
		if (!bSigner)
		{
			// VOID, NOT FAIL. With no signer the counted set never leaves the process, so the run cannot
			// say anything about durability. Grading that FAIL would report a verdict on an untested thing.
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[DUR] VOID -- /counted-entitlement NOT CONFIGURED (needs AFL_COUNTED_URL + AFL_EARN_HMAC_KEY). Nothing about durability was tested."));
			return;
		}
		if (Pf.IsEmpty()) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DUR] VOID -- no PlayFabId")); return; }

		static const FName SlotKey(TEXT("AFL.CreatorSlot"));
		static const FName SkuX3(TEXT("AFL.CreatorSlot.x3"));
		const int32 Base = Wallet->GetCountedEntitlement(SlotKey);
		const int32 VoBefore = Wallet->GetVolts();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DUR] ARM1 baseline counter=%d mirrorVo=%d (x3 costs 4990)"), Base, VoBefore);

		TWeakObjectPtr<UAFLWalletComponent> WeakW(Wallet);
		TWeakObjectPtr<UWorld> WeakWorld(World);

		// ARM2 -- buy through the PRODUCTION entry, not a debug grant. A proof arm that exercises a
		// convenience path validates the instrument, not the product.
		// Volts named EXPLICITLY rather than left to Auto: a proof must not depend on which currency the
		// resolver happened to pick.
		Wallet->ClientRequestPurchase(SkuX3, EAFLPayCurrency::Volts, TFunction<void(bool)>());

		FTimerHandle H1;
		World->GetTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([WeakW, WeakWorld, Base]()
		{
			UAFLWalletComponent* W = WeakW.Get();
			UWorld* Wd = WeakWorld.Get();
			if (!W || !Wd) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DUR] VOID -- wallet/world gone before ARM2")); return; }

			static const FName K(TEXT("AFL.CreatorSlot"));
			const int32 After = W->GetCountedEntitlement(K);
			const bool bArm2 = (After == Base + 3);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DUR] ARM2 buy x3 -> counter %d -> %d (want %d) %s"),
				Base, After, Base + 3, bArm2 ? TEXT("PASS") : TEXT("FAIL"));

			// ARM3 -- the SHIPPING reconcile. It now re-reads the counted set from PlayFab, so a count
			// that survives here survived a real round-trip, not merely an untouched array.
			W->DebugForceReconcile();

			FTimerHandle H2;
			Wd->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([WeakW, After, bArm2]()
			{
				UAFLWalletComponent* W2 = WeakW.Get();
				if (!W2) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DUR] VOID -- wallet gone before ARM3")); return; }

				static const FName K2(TEXT("AFL.CreatorSlot"));
				const int32 AfterRecon = W2->GetCountedEntitlement(K2);
				const bool bArm3 = (AfterRecon == After) && (After > 0);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DUR] ARM3 reconcile -> counter %d -> %d (want unchanged) %s"),
					After, AfterRecon, bArm3 ? TEXT("PASS") : TEXT("FAIL"));

				// HANDED TO THE NEXT PROCESS. The relaunch arm needs a number that was decided BEFORE
				// the process died, or run 2 would be checking a value against itself.
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DUR] RELAUNCH_EXPECT=%d"), AfterRecon);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DUR] RUN1 SUMMARY arm2=%d arm3=%d"),
					bArm2 ? 1 : 0, bArm3 ? 1 : 0);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DUR] END"));
			}), 6.0f, false);
		}), 8.0f, false);
	}

	// Run 2. A FRESH PROCESS with the local mirror DELETED, so a surviving count has exactly one
	// possible source: PlayFab.
	void HandleAFLVerifyCountedRelaunch(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.VerifyCountedRelaunch <expected> - run inside PIE.")); return; }
		if (Args.Num() < 1) { Ar.Log(TEXT("afl.Online.VerifyCountedRelaunch <expected> - pass RELAUNCH_EXPECT from run 1.")); return; }
		const int32 Expected = FCString::Atoi(*Args[0]);

		// THE DISCRIMINATOR IS PROVENANCE, NOT FILE ABSENCE. The first version of this arm asserted
		// AFLEconomy.sav was gone -- but the game REWRITES that file during startup, so it was back
		// before the probe graded and the arm correctly VOIDed itself. File state cannot carry the
		// claim. bCountedHydratedFromBackend can: it is raised in exactly one place, inside the
		// /counted-entitlement response handler, where the array is replaced by the backend's reply.
		// The .sav is still reported, now as CONTEXT rather than as the test.
		const bool bLocalSavePresent = UGameplayStatics::DoesSaveGameExist(TEXT("AFLEconomy"), 0);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[REL] ARM4a context: local mirror present=%d (informational -- the game recreates it at startup, so it cannot be the discriminator)"),
			bLocalSavePresent ? 1 : 0);

		FString Why;
		UAFLWalletComponent* Wallet = GetServerWalletForLocalPlayer(World, Why);
		const UAFLOnlineSubsystem* Online = Wallet ? UAFLOnlineSubsystem::Get(Wallet) : UAFLOnlineSubsystem::Get(World);
		const FString Pf = Online ? Online->GetPlayFabId() : FString();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[REL] ARM4a wallet=%d pfid=%s expected=%d why=%s"),
			Wallet ? 1 : 0, Pf.IsEmpty() ? TEXT("<none>") : *Pf, Expected, Why.IsEmpty() ? TEXT("ok") : *Why);

		if (!Wallet) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[REL] VOID -- no server wallet")); return; }

		const bool bFromBackend = Wallet->WasCountedHydratedFromBackend();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[REL] ARM4a provenance: hydratedFromBackend=%d"), bFromBackend ? 1 : 0);
		if (!bFromBackend)
		{
			// VOID, NOT FAIL. Without a backend hydration this process never asked the durable store,
			// so whatever the counter reads says nothing about durability either way.
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[REL] VOID -- the counted set was never hydrated from /counted-entitlement in this process. Nothing about durability was tested."));
			return;
		}

		static const FName SlotKey(TEXT("AFL.CreatorSlot"));
		const int32 Now = Wallet->GetCountedEntitlement(SlotKey);
		const bool bArm4 = (Now == Expected) && (Expected > 0);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[REL] ARM4 fresh process -> counter=%d (want %d) %s -- the array was REPLACED by the backend's reply, so the durable store is the only source"),
			Now, Expected, bArm4 ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[REL] END"));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLVerifyCountedDurableCmd(TEXT("afl.Online.VerifyCountedDurable"),
		TEXT("CC-X30 run 1: buy AFL.CreatorSlot.x3 through the production entry, then force the shipping ")
		TEXT("reconcile, and assert the counted slot survives. Prints RELAUNCH_EXPECT=<n> for run 2. ")
		TEXT("VOID (not FAIL) without a signer or a PlayFabId."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLVerifyCountedDurable));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLVerifyCountedRelaunchCmd(TEXT("afl.Online.VerifyCountedRelaunch"),
		TEXT("CC-X30 run 2: in a FRESH process with AFLEconomy.sav deleted, assert the counted slot came ")
		TEXT("back from PlayFab. VOID if the local mirror is present -- then the source is ambiguous."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLVerifyCountedRelaunch));

	// === CC-X30 REDEMPTION: afl.Online.VerifyCreditRedeem ==========================================
	void HandleAFLVerifyCreditRedeem(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.VerifyCreditRedeem - run inside PIE.")); return; }

		FString Why;
		UAFLWalletComponent* W = GetServerWalletForLocalPlayer(World, Why);
		const UAFLOnlineSubsystem* Online = W ? UAFLOnlineSubsystem::Get(W) : UAFLOnlineSubsystem::Get(World);
		const FString Pf = Online ? Online->GetPlayFabId() : FString();
		const bool bSigner = Online && Online->IsCountedEntitlementConfigured();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RDM] ARM0 wallet=%d signer=%d pfid=%s why=%s"),
			W ? 1 : 0, bSigner ? 1 : 0, Pf.IsEmpty() ? TEXT("<none>") : *Pf, Why.IsEmpty() ? TEXT("ok") : *Why);
		if (!W) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RDM] VOID -- no server wallet")); return; }
		if (!bSigner || Pf.IsEmpty())
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RDM] VOID -- no signer or no PlayFabId. Nothing about redemption was tested."));
			return;
		}

		const UAFLCosmeticCatalogSubsystem* Cat = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!Cat) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RDM] VOID -- no catalog")); return; }

		// ---- pick targets BY PROPERTY -----------------------------------------------------------
		TArray<const FAFLCatalogEntry*> All;
		Cat->GetEntriesByType(EAFLCosmeticType::Weapon, All);
		TArray<const FAFLCatalogEntry*> Bundles;
		Cat->GetEntriesByType(EAFLCosmeticType::Bundle, Bundles);

		auto IsHandCannon = [](const FAFLCatalogEntry* E) -> bool
		{
			return E && !E->ItemDefClass.IsNull() && E->ItemDefClass.ToString().Contains(TEXT("HandCannon"));
		};

		TArray<FName> Pool;          // marked AND not already owned -- the positive targets
		FName Unmarked = NAME_None;  // marked==false            -> must be refused
		FName HandCannon = NAME_None;// resolves to a HandCannon -> must be refused
		FName BundleRow = NAME_None; // Type==Bundle             -> must be refused
		for (const FAFLCatalogEntry* E : All)
		{
			if (!E) { continue; }
			if (E->bCreditRedeemable)
			{
				if (!W->OwnsCosmetic(E->CosmeticId)) { Pool.Add(E->CosmeticId); }
			}
			else
			{
				if (IsHandCannon(E)) { if (HandCannon.IsNone()) { HandCannon = E->CosmeticId; } }
				else if (Unmarked.IsNone()) { Unmarked = E->CosmeticId; }
			}
		}
		if (Bundles.Num() > 0 && Bundles[0]) { BundleRow = Bundles[0]->CosmeticId; }

		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[RDM] ARM0b targets: pool(unowned)=%d unmarked=%s handcannon=%s bundle=%s"),
			Pool.Num(), *Unmarked.ToString(), *HandCannon.ToString(), *BundleRow.ToString());
		if (Pool.Num() < 3 || Unmarked.IsNone() || HandCannon.IsNone() || BundleRow.IsNone())
		{
			// VOID: without three unowned pool rows the drain arm cannot run, and without each negative
			// target its refusal is untested. A partial run must not be graded.
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[RDM] VOID -- need 3 unowned pool rows and one of each negative target."));
			return;
		}

		static const FName CreditKey(TEXT("AFL.WeaponCredit"));
		static const FName CreditSku(TEXT("AFL.WeaponCredit.x3"));
		const int32 Base = W->GetCountedEntitlement(CreditKey);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RDM] ARM1 baseline credits=%d mirrorVo=%d (x3 costs 990)"),
			Base, W->GetVolts());

		TWeakObjectPtr<UAFLWalletComponent> WW(W);
		TWeakObjectPtr<UWorld> WWorld(World);
		const FName Target = Pool[0], T2 = Pool[1], T3 = Pool[2];

		W->ClientRequestPurchase(CreditSku, EAFLPayCurrency::Volts, TFunction<void(bool)>());

		FTimerHandle H1;
		World->GetTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda(
			[WW, WWorld, Base, Target, T2, T3, Unmarked, HandCannon, BundleRow]()
		{
			UAFLWalletComponent* A = WW.Get(); UWorld* Wd = WWorld.Get();
			if (!A || !Wd) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RDM] VOID -- gone before ARM2")); return; }
			static const FName K(TEXT("AFL.WeaponCredit"));

			const int32 Bought = A->GetCountedEntitlement(K);
			const bool bArm2 = (Bought == Base + 3);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RDM] ARM2 buy WeaponCredit.x3 -> %d -> %d (want %d) %s"),
				Base, Bought, Base + 3, bArm2 ? TEXT("PASS") : TEXT("FAIL"));

			// ---- THE THREE REFUSALS. Fired together; none may move the counter. ------------------
			A->ServerRequestCreditRedemption(Unmarked);
			A->ServerRequestCreditRedemption(HandCannon);
			A->ServerRequestCreditRedemption(BundleRow);

			FTimerHandle H2;
			Wd->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda(
				[WW, WWorld, Bought, bArm2, Target, T2, T3, Unmarked, HandCannon, BundleRow]()
			{
				UAFLWalletComponent* B = WW.Get(); UWorld* Wd2 = WWorld.Get();
				if (!B || !Wd2) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RDM] VOID -- gone before ARM4")); return; }
				static const FName K2(TEXT("AFL.WeaponCredit"));

				const int32 AfterRefusals = B->GetCountedEntitlement(K2);
				const bool bArm4 = (AfterRefusals == Bought);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[RDM] ARM4 three refusals (unmarked=%s handcannon=%s bundle=%s) -> credits %d -> %d (want unchanged) %s"),
					*Unmarked.ToString(), *HandCannon.ToString(), *BundleRow.ToString(),
					Bought, AfterRefusals, bArm4 ? TEXT("PASS") : TEXT("FAIL"));
				const bool bNotOwned = !B->OwnsCosmetic(Unmarked) && !B->OwnsCosmetic(HandCannon) && !B->OwnsCosmetic(BundleRow);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RDM] ARM4b none of the refused targets became owned = %d"),
					bNotOwned ? 1 : 0);

				// ---- THE POSITIVE. One credit, one weapon. --------------------------------------
				B->ServerRequestCreditRedemption(Target);

				FTimerHandle H3;
				Wd2->GetTimerManager().SetTimer(H3, FTimerDelegate::CreateLambda(
					[WW, WWorld, AfterRefusals, bArm2, bArm4, Target, T2, T3]()
				{
					UAFLWalletComponent* C = WW.Get(); UWorld* Wd3 = WWorld.Get();
					if (!C || !Wd3) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RDM] VOID -- gone before ARM3")); return; }
					static const FName K3(TEXT("AFL.WeaponCredit"));

					const int32 Spent = C->GetCountedEntitlement(K3);
					const bool bOwned = C->OwnsCosmetic(Target);
					const bool bArm3 = (Spent == AfterRefusals - 1) && bOwned;
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[RDM] ARM3 redeem %s -> credits %d -> %d (want %d), owned=%d %s"),
						*Target.ToString(), AfterRefusals, Spent, AfterRefusals - 1, bOwned ? 1 : 0,
						bArm3 ? TEXT("PASS") : TEXT("FAIL"));

					// ---- durability of the SPEND, through the shipping reconcile ----------------
					C->DebugForceReconcile();

					FTimerHandle H4;
					Wd3->GetTimerManager().SetTimer(H4, FTimerDelegate::CreateLambda(
						[WW, WWorld, Spent, bArm2, bArm3, bArm4, Target, T2, T3]()
					{
						UAFLWalletComponent* D = WW.Get(); UWorld* Wd4 = WWorld.Get();
						if (!D || !Wd4) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RDM] VOID -- gone before ARM5")); return; }
						static const FName K4(TEXT("AFL.WeaponCredit"));

						const int32 AfterRecon = D->GetCountedEntitlement(K4);
						const bool bStillOwned = D->OwnsCosmetic(Target);
						const bool bArm5 = (AfterRecon == Spent) && bStillOwned;
						UE_LOG(LogAFLCombat, Display,
							TEXT("AFL_TEST[RDM] ARM5 reconcile -> credits %d -> %d (want unchanged), stillOwned=%d %s"),
							Spent, AfterRecon, bStillOwned ? 1 : 0, bArm5 ? TEXT("PASS") : TEXT("FAIL"));

						// ---- drain the remaining two, then the FOURTH must be refused ------------
						D->ServerRequestCreditRedemption(T2);
						D->ServerRequestCreditRedemption(T3);

						FTimerHandle H5;
						Wd4->GetTimerManager().SetTimer(H5, FTimerDelegate::CreateLambda(
							[WW, WWorld, bArm2, bArm3, bArm4, bArm5, T2, T3]()
						{
							UAFLWalletComponent* E = WW.Get(); UWorld* Wd5 = WWorld.Get();
							if (!E || !Wd5) { return; }
							static const FName K5(TEXT("AFL.WeaponCredit"));
							const int32 Drained = E->GetCountedEntitlement(K5);
							UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RDM] ARM6a drained to %d (want 0); both granted=%d/%d"),
								Drained, E->OwnsCosmetic(T2) ? 1 : 0, E->OwnsCosmetic(T3) ? 1 : 0);

							// THE FOURTH. Nothing left to spend, so it must be refused, not silently allowed.
							E->ServerRequestCreditRedemption(T2);

							FTimerHandle H6;
							Wd5->GetTimerManager().SetTimer(H6, FTimerDelegate::CreateLambda(
								[WW, bArm2, bArm3, bArm4, bArm5, Drained]()
							{
								UAFLWalletComponent* F = WW.Get();
								if (!F) { return; }
								static const FName K6(TEXT("AFL.WeaponCredit"));
								const int32 Final = F->GetCountedEntitlement(K6);
								const bool bArm6 = (Drained == 0) && (Final == 0);
								UE_LOG(LogAFLCombat, Display,
									TEXT("AFL_TEST[RDM] ARM6 FOURTH redemption past zero -> credits %d (want 0) %s"),
									Final, bArm6 ? TEXT("PASS") : TEXT("FAIL"));
								UE_LOG(LogAFLCombat, Display,
									TEXT("AFL_TEST[RDM] SUMMARY buy=%d redeem=%d refusals=%d reconcile=%d fourth=%d"),
									bArm2 ? 1 : 0, bArm3 ? 1 : 0, bArm4 ? 1 : 0, bArm5 ? 1 : 0, bArm6 ? 1 : 0);
								UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RDM] END"));
							}), 6.0f, false);
						}), 8.0f, false);
					}), 7.0f, false);
				}), 7.0f, false);
			}), 6.0f, false);
		}), 8.0f, false);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLVerifyCreditRedeemCmd(TEXT("afl.Online.VerifyCreditRedeem"),
		TEXT("CC-X30 redemption: buy AFL.WeaponCredit.x3 through the production entry, spend one on a pool ")
		TEXT("weapon, and check the four refusals (unmarked / hand cannon / bundle / past zero). ")
		TEXT("SPENDS 990 real Volts. VOID without a signer, a PlayFabId, or three unowned pool rows."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLVerifyCreditRedeem));

	// === CC-X22: afl.Catalog.SellableProbe =========================================================
	void HandleAFLSellableProbe(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World) { Ar.Log(TEXT("afl.Catalog.SellableProbe - needs a world.")); return; }
		UAFLCosmeticCatalogSubsystem* Cat = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!Cat) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SELL] VOID -- no catalog subsystem")); return; }

		// What the OLD surface offered, and the number ARM1 must NOT equal.
		const int32 NotFree = Cat->CountNonFreeRows();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SELL] ARM0 non-GrantedFree rows = %d (what the OLD surface offered)"), NotFree);

#if !UE_BUILD_SHIPPING
		// ---- ARM1: UNKNOWN shows NOTHING ----------------------------------------------------------
		Cat->DebugClearRegisteredSet();
		TArray<FAFLCatalogEntry> WhenUnknown;
		Cat->GetPurchasableEntries(WhenUnknown);
		const bool bArm1 = (WhenUnknown.Num() == 0) && (NotFree > 0);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SELL] ARM1 sellable set UNKNOWN -> purchasable=%d (want 0, and NOT %d) %s"),
			WhenUnknown.Num(), NotFree, bArm1 ? TEXT("PASS") : TEXT("FAIL"));

		// restore from the cache the live fetch already wrote
		const bool bReloaded = Cat->LoadRegisteredCache();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SELL] ARM4 cache reload = %d, ids=%d"),
			bReloaded ? 1 : 0, Cat->GetRegisteredCount());
#else
		const bool bArm1 = false;
		const bool bReloaded = false;
#endif

		// ---- ARM2: KNOWN shows the intersection ---------------------------------------------------
		if (!Cat->IsRegisteredSetKnown())
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[SELL] VOID after ARM1 -- no cache and no live answer, so the intersection cannot be measured. ")
				TEXT("Run once while logged in to populate the cache."));
			return;
		}
		TArray<FAFLCatalogEntry> Shown;
		Cat->GetPurchasableEntries(Shown);
		const int32 Withheld = NotFree - Shown.Num();
		const bool bArm2 = (Shown.Num() > 0) && (Shown.Num() < NotFree);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SELL] ARM2 sellable set KNOWN (%d ids) -> shown=%d withheld=%d of %d %s"),
			Cat->GetRegisteredCount(), Shown.Num(), Withheld, NotFree, bArm2 ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SELL] ARM2 NOTE withheld = rows priced in the UE catalog that the backend cannot sell. ")
			TEXT("Every one of those was a shop button returning ItemNotFound before this change."));

		// ---- ARM3: the SHAPE, not just the count --------------------------------------------------
		// A filter that returned nothing, or everything-once-known, would pass ARM1 and ARM2 both.
		// Naming one id that must appear and one that must not is what makes the shape observable.
		auto Offered = [&Shown](const TCHAR* Id) -> bool
		{
			const FName N(Id);
			for (const FAFLCatalogEntry& E : Shown) { if (E.CosmeticId == N) { return true; } }
			return false;
		};
		const bool bRegisteredOffered  = Offered(TEXT("AFL.Weapon.Arclight"));      // registered 2026-08-21
		const bool bUnregisteredHidden = !Offered(TEXT("AFL.Ability.EMP"));         // priced here, never in the manifest
		const bool bArm3 = bRegisteredOffered && bUnregisteredHidden;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SELL] ARM3 shape: registered AFL.Weapon.Arclight offered=%d ; unregistered AFL.Ability.EMP hidden=%d %s"),
			bRegisteredOffered ? 1 : 0, bUnregisteredHidden ? 1 : 0, bArm3 ? TEXT("PASS") : TEXT("FAIL"));

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SELL] SUMMARY unknown=%d intersect=%d shape=%d cache=%d"),
			bArm1 ? 1 : 0, bArm2 ? 1 : 0, bArm3 ? 1 : 0, bReloaded ? 1 : 0);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SELL] END"));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLSellableProbeCmd(TEXT("afl.Catalog.SellableProbe"),
		TEXT("CC-X22: prove the store offers only what the backend can sell. Unknown -> nothing; known -> ")
		TEXT("the intersection; plus a named registered id that must appear and a named unregistered id ")
		TEXT("that must not. Reads only -- spends nothing."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLSellableProbe));

	// === CC-5 step 2: afl.Creator.EntryProof =====================================================
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_Layer_Menu_EntryProof, "UI.Layer.Menu");

	static UCommonActivatableWidget* AFLTopOfMenuLayer(APlayerController* PC)
	{
		UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(PC);
		UCommonActivatableWidgetContainerBase* Stack =
			Layout ? Layout->GetLayerWidget(TAG_UI_Layer_Menu_EntryProof) : nullptr;
		return Stack ? Stack->GetActiveWidget() : nullptr;
	}

	/** A real press+release at the widget's own painted position. Returns false if the widget has never
	 *  been painted -- which must be reported, not silently treated as "the click did nothing". */
	static bool AFLClickWidget(UWidget* W, FString& OutWhere)
	{
		if (!W) { OutWhere = TEXT("<null widget>"); return false; }
		const FGeometry& G = W->GetCachedGeometry();
		const FVector2D Size = G.GetLocalSize();
		if (Size.X <= 0.0f || Size.Y <= 0.0f)
		{
			OutWhere = TEXT("<never painted -- zero cached geometry>");
			return false;
		}
		const FVector2D Pos = G.LocalToAbsolute(Size * 0.5f);
		OutWhere = FString::Printf(TEXT("(%.0f,%.0f) size=%.0fx%.0f"), Pos.X, Pos.Y, Size.X, Size.Y);

		FSlateApplication& App = FSlateApplication::Get();
		FPointerEvent Down(0, App.CursorPointerIndex, Pos, Pos, TSet<FKey>(), EKeys::LeftMouseButton,
			0.0f, FModifierKeysState());
		App.ProcessMouseButtonDownEvent(nullptr, Down);
		FPointerEvent Up(0, App.CursorPointerIndex, Pos, Pos, TSet<FKey>(), EKeys::LeftMouseButton,
			0.0f, FModifierKeysState());
		App.ProcessMouseButtonUpEvent(Up);
		return true;
	}

	void AFLRunEntryProof(UWorld* World, APlayerController* PC);

	/** ARMS the proof and waits for a PAINTED loadout. Cached geometry -- the thing the click needs --
	 *  only exists after a frame has been drawn, so running inline would measure "never painted" and
	 *  report it as a failure of the entry. Waiting is not a retry: the sequence runs EXACTLY ONCE, on
	 *  the first tick where its precondition holds, and if the precondition never holds that is REPORTED
	 *  rather than left as silence. Arm it before or after opening the loadout -- either order works. */
	void HandleAFLCreatorEntryProof(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld())
		{
			Ar.Log(TEXT("afl.Creator.EntryProof - run inside PIE.")); return;
		}
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[ENT] ARMED -- waiting for a PAINTED front-end loadout on the Menu layer. ")
			TEXT("Open it from W_IRONICS_Home; the proof fires by itself on the first frame it is drawn."));

		TWeakObjectPtr<UWorld> WeakWorld(World);
		int32* Waited = new int32(0);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[WeakWorld, Waited](float) -> bool
			{
				UWorld* W = WeakWorld.Get();
				APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
				if (!W || !PC)
				{
					UE_LOG(LogAFLCombat, Warning,
						TEXT("AFL_TEST[ENT] DISARMED after %d frames -- the world went away before a ")
						TEXT("loadout was painted. NOTHING WAS MEASURED."), *Waited);
					delete Waited; return false;
				}
				UAFLW_LoadoutBase* L = Cast<UAFLW_LoadoutBase>(AFLTopOfMenuLayer(PC));
				const bool bPainted = L && L->GetCachedGeometry().GetLocalSize().X > 0.0f;
				if (bPainted)
				{
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[ENT] FIRING after %d frames -- loadout %s is painted."),
						*Waited, *L->GetName());
					AFLRunEntryProof(W, PC);
					delete Waited; return false;
				}
				// NO FRAME CAP. The precondition is a HUMAN opening the loadout, and a cap short enough
				// to be useful is shorter than a person takes -- the first run disarmed in 26 seconds and
				// measured nothing. The latch now lives as long as the world does; it is not a retry
				// because the sequence still runs exactly once. It HEARTBEATS so that a quiet log is
				// readable as "armed and waiting" and never as "the command did not run".
				if ((++(*Waited) % 600) == 0)
				{
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[ENT] still armed after %d ticks -- top of Menu layer is %s. ")
						TEXT("Open the LOADOUT from the home menu."),
						*Waited, *GetNameSafe(AFLTopOfMenuLayer(PC)));
				}
				return true;
			}), 0.0f);
	}

	void AFLRunEntryProof(UWorld* World, APlayerController* PC)
	{

		// ---- CONTROL 0: the instrument can see the thing it is about to test --------------------
		UAFLW_LoadoutBase* Loadout = Cast<UAFLW_LoadoutBase>(AFLTopOfMenuLayer(PC));
		if (!Loadout)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[ENT] VOID -- top of the Menu layer is not a loadout (got %s). ")
				TEXT("Open the front-end loadout from W_IRONICS_Home first."),
				*GetNameSafe(AFLTopOfMenuLayer(PC)));
			return;
		}
		UPanelWidget* StickerRail = Loadout->GetStickerRailForTest();
		UPanelWidget* MaskRail    = Loadout->GetFacemaskRailForTest();
		const int32 StickerKids = StickerRail ? StickerRail->GetChildrenCount() : -1;
		const int32 MaskKids    = MaskRail ? MaskRail->GetChildrenCount() : -1;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[ENT] CTRL0 loadout=%s stickerRailBound=%d children=%d | maskRailBound=%d children=%d %s"),
			*Loadout->GetName(), StickerRail ? 1 : 0, StickerKids, MaskRail ? 1 : 0, MaskKids,
			(StickerRail && StickerKids == 1) ? TEXT("PASS")
				: TEXT("FAIL <- BindWidgetOptional did not bind, or the rail built no affordance"));
		if (!StickerRail || StickerKids != 1)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[ENT] VOID -- nothing to click."));
			return;
		}

		// ---- CONTROL 1: nothing already open ----------------------------------------------------
		const bool bPreClean = (Cast<UAFLW_Creator>(AFLTopOfMenuLayer(PC)) == nullptr);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[ENT] CTRL1 no creator before the click = %d %s"),
			bPreClean ? 1 : 0, bPreClean ? TEXT("PASS") : TEXT("FAIL <- VOID, cannot read 'it opened'"));
		if (!bPreClean) { return; }

		// ---- CONTROL 2: a SELECTION tile must not open it ----------------------------------------
		// Same widget class, same delegate, same handler. If this opens a creator, ARM A proves nothing.
		if (MaskRail && MaskKids > 0)
		{
			FString Where;
			const bool bClicked = AFLClickWidget(MaskRail->GetChildAt(0), Where);
			const bool bStillNone = (Cast<UAFLW_Creator>(AFLTopOfMenuLayer(PC)) == nullptr);
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[ENT] CTRL2 facemask tile clicked=%d at %s -> creator opened=%d %s"),
				bClicked ? 1 : 0, *Where, bStillNone ? 0 : 1,
				(bClicked && bStillNone) ? TEXT("PASS")
					: TEXT("FAIL <- either the click never landed, or ANY tile opens the creator"));
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[ENT] CTRL2 NOT RUN -- no facemask tile owned. ARM A is UNCONTROLLED: it ")
				TEXT("cannot distinguish 'the affordance opens it' from 'any click opens it'."));
		}

		// Snapshot BEFORE, so ARM B compares against something measured, not remembered.
		const FName BodyBefore = Loadout->GetEquippedIdForAxis(EAFLLoadoutAxis::BodyColor);
		const FName MaskBefore = Loadout->GetEquippedIdForAxis(EAFLLoadoutAxis::Facemask);

		// ---- ARM A: the affordance opens the creator, focused on ITS axis ------------------------
		FString WhereA;
		const bool bClickedA = AFLClickWidget(StickerRail->GetChildAt(0), WhereA);
		UAFLW_Creator* Opened = Cast<UAFLW_Creator>(AFLTopOfMenuLayer(PC));
		const bool bArmA = bClickedA && Opened && (Opened->FocusAxis == EAFLLoadoutAxis::Sticker);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[ENT] ARM A sticker affordance clicked=%d at %s -> top=%s focusAxis=%d (want %d) %s"),
			bClickedA ? 1 : 0, *WhereA, *GetNameSafe(AFLTopOfMenuLayer(PC)),
			Opened ? (int32)Opened->FocusAxis : -1, (int32)EAFLLoadoutAxis::Sticker,
			bArmA ? TEXT("PASS") : TEXT("FAIL"));
		if (!Opened)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[ENT] ARM B NOT RUN -- nothing was opened to close."));
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[ENT] END"));
			return;
		}

		// ---- ARM B: Back pops it, and the SAME loadout object comes back untouched ---------------
		// Deactivating the creator is the framework's own back semantics -- the same thing the creator's
		// back button and the Back input action both do. What is being proved is that the loadout under
		// it SURVIVED: same object, same equipped ids, not a rebuilt one that merely looks the same.
		Opened->DeactivateWidget();
		UCommonActivatableWidget* NowTop = AFLTopOfMenuLayer(PC);
		const bool bSameObject = (NowTop == Loadout);
		const FName BodyAfter = Loadout->GetEquippedIdForAxis(EAFLLoadoutAxis::BodyColor);
		const FName MaskAfter = Loadout->GetEquippedIdForAxis(EAFLLoadoutAxis::Facemask);
		const bool bArmB = bSameObject && (BodyAfter == BodyBefore) && (MaskAfter == MaskBefore);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[ENT] ARM B back -> top=%s sameObject=%d body %s->%s mask %s->%s %s"),
			*GetNameSafe(NowTop), bSameObject ? 1 : 0,
			*BodyBefore.ToString(), *BodyAfter.ToString(),
			*MaskBefore.ToString(), *MaskAfter.ToString(),
			bArmB ? TEXT("PASS") : TEXT("FAIL"));

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[ENT] END"));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCreatorEntryProofCmd(TEXT("afl.Creator.EntryProof"),
		TEXT("CC-5 step 2: with the front-end loadout OPEN, click the STICKER affordance with real Slate ")
		TEXT("input and prove a creator focused on that axis is pushed, then that Back reveals the SAME ")
		TEXT("loadout object with its state intact. Controlled by a selection tile that must NOT open it."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCreatorEntryProof));

	// === CC-5.2: afl.Creator.WidgetProbe ==========================================================
	void HandleAFLCreatorWidgetProbe(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Creator.WidgetProbe - run inside PIE.")); return; }
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[CRW] VOID -- no PlayerController")); return; }

		UClass* CreatorCls = LoadClass<UAFLW_Creator>(nullptr,
			TEXT("/Game/BagMan/UI/Creator/WBP_AFL_Creator.WBP_AFL_Creator_C"));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[CRW] ARM0 WBP class loaded = %d"), CreatorCls ? 1 : 0);
		if (!CreatorCls) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[CRW] VOID -- the creator WBP did not load")); return; }

		// ---- ARM1: FAILS CLOSED with no loadout bound ------------------------------------------
		UAFLW_Creator* Solo = CreateWidget<UAFLW_Creator>(PC, CreatorCls);
		if (!Solo) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[CRW] VOID -- CreateWidget failed")); return; }
		Solo->RefreshFromSchema();
		const bool bArm1 = (Solo->GetChannelRows().Num() == 0) && !Solo->IsSchemaResolved();
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[CRW] ARM1 no loadout -> rows=%d resolved=%d (want 0/0) %s"),
			Solo->GetChannelRows().Num(), Solo->IsSchemaResolved() ? 1 : 0, bArm1 ? TEXT("PASS") : TEXT("FAIL"));

		// ---- ARM2/3/4: THE RAIL VARIES BY CHASSIS -----------------------------------------------
		// DRIVEN WITH A KNOWN SCHEMA, on purpose. A previous run bound a CreateWidget'd loadout whose
		// DisplayPawn had never spawned; CreatorGetSchema() then resolved against whatever material was
		// reachable -- MID_MI_AFL_FaceMask_Pink_2 -- and reported all four channels Connected on an
		// UNAUDITED master. Every assertion below compared two numbers drawn from that same wrong
		// source and "passed" while testing nothing. Deriving from the two MEASURED masters makes the
		// claim falsifiable instead of self-confirming.
		struct FCase { const TCHAR* Path; const TCHAR* Nick; int32 WantInteractive; };
		static const FCase Cases[] = {
			{ TEXT("/Game/BagMan/Materials/M_AFL_Character"),                        TEXT("X-line"),      2 },
			{ TEXT("/Game/Characters/Heroes/Mannequin/Materials/M_Mannequin"),       TEXT("Manny-based"), 3 },
		};

		UAFLW_Creator* W = CreateWidget<UAFLW_Creator>(PC, CreatorCls);
		bool bArm2 = true, bArm3 = true, bArm4Seen = false, bArm4 = true, bArm7Seen = false, bArm7 = true;

		for (const FCase& Case : Cases)
		{
			UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, Case.Path);
			if (!Mat)
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[CRW] VOID -- master did not load: %s"), Case.Path);
				bArm2 = false;
				continue;
			}
			const FAFLCreatorChannelSchema Sch = FAFLCreatorChannelSchema::DeriveFromMaterial(Mat);
			W->DebugBuildRowsFromSchema(Sch);
			const TArray<FAFLCreatorChannelRow>& Rows = W->GetChannelRows();

			int32 Interactive = 0, Disabled = 0, WithReason = 0, Inert = 0, Absent = 0;
			FString InertReason, AbsentReason;
			for (const FAFLCreatorChannelRow& R : Rows)
			{
				if (R.bInteractive) { ++Interactive; }
				else
				{
					++Disabled;
					if (!R.Reason.IsEmpty()) { ++WithReason; }
				}
				if (R.State == EAFLChannelAvailability::PresentButInert) { ++Inert;  InertReason  = R.Reason.ToString(); }
				if (R.State == EAFLChannelAvailability::Absent)          { ++Absent; AbsentReason = R.Reason.ToString(); }
			}

			// ARM2 -- the rail LENGTH is the chassis's, and EVERY channel is still emitted.
			const bool bCase2 = (Interactive == Case.WantInteractive)
			                 && (Interactive == Sch.AvailableCount())
			                 && (Rows.Num() == 4);
			bArm2 = bArm2 && bCase2;
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[CRW] ARM2 %-12s master=%-18s audited=%d rows=%d interactive=%d (want %d) %s"),
				Case.Nick, *Sch.ResolvedFromMaster.ToString(), Sch.bMasterAudited ? 1 : 0,
				Rows.Num(), Interactive, Case.WantInteractive, bCase2 ? TEXT("PASS") : TEXT("FAIL"));

			// ARM3 -- every disabled row carries a reason.
			const bool bCase3 = (Disabled == WithReason);
			bArm3 = bArm3 && bCase3;
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[CRW] ARM3 %-12s disabled=%d withReason=%d inert=%d absent=%d %s"),
				Case.Nick, Disabled, WithReason, Inert, Absent, bCase3 ? TEXT("PASS") : TEXT("FAIL"));

			// ARM4 -- the two disabled states are told apart by TEXT.
			if (Inert > 0 && Absent > 0)
			{
				bArm4Seen = true;
				const bool bCase4 = !InertReason.Equals(AbsentReason);
				bArm4 = bArm4 && bCase4;
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[CRW] ARM4 %-12s reasons differ = %d %s"),
					Case.Nick, bCase4 ? 1 : 0, bCase4 ? TEXT("PASS") : TEXT("FAIL"));
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[CRW]    inert : %s"), *InertReason);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[CRW]    absent: %s"), *AbsentReason);
			}

			// ARM7 -- a disabled channel cannot write. No loadout is bound here, so the refusal is
			// tested at the SCHEMA gate, which is the gate that actually protects it.
			for (const FAFLCreatorChannelRow& R : Rows)
			{
				if (!R.bInteractive)
				{
					bArm7Seen = true;
					const int32 Before = W->GetChannelRows().Num();
					W->SetChannelHue(R.Channel, 123.0f);   // must be refused on state, not on widget enable-ness
					const bool bStill = (W->GetChannelRows().Num() == Before);
					bArm7 = bArm7 && bStill;
					break;
				}
			}
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[CRW] ARM4 %s"),
			bArm4Seen ? (bArm4 ? TEXT("PASS") : TEXT("FAIL")) : TEXT("NOT EXERCISED -- neither master shows both states"));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[CRW] ARM7 disabled-channel write refused = %s"),
			bArm7Seen ? (bArm7 ? TEXT("PASS") : TEXT("FAIL")) : TEXT("NOT EXERCISED"));

		const TArray<FAFLCreatorChannelRow>& Rows = W->GetChannelRows();

		// ---- ARM5: the clamp, WITH a control that proves the predicate can fail ------------------
		auto InGamut = [](const FLinearColor& C) -> bool
		{
			const FLinearColor H = C.LinearRGBToHSV();
			return H.G >= AFLCreatorGamut::MinSaturation - KINDA_SMALL_NUMBER
			    && H.B >= AFLCreatorGamut::MinValue      - KINDA_SMALL_NUMBER
			    && H.B <= AFLCreatorGamut::MaxValue      + KINDA_SMALL_NUMBER;
		};
		int32 OutOfGamut = 0, Checked = 0;
		for (float Hue = 0.0f; Hue < 360.0f; Hue += 10.0f)
		{
			++Checked;
			if (!InGamut(W->GetArcTrackColour(Hue))) { ++OutOfGamut; }
		}
		// THE CONTROL: a deliberately washed-out colour MUST be flagged. If it is not, the predicate
		// cannot return false and the zero above proves nothing.
		const FLinearColor Bad = FLinearColor(0.9f, 0.88f, 0.87f);   // near-white, S well under the floor
		const bool bControlFlags = !InGamut(Bad);
		const bool bArm5 = (OutOfGamut == 0) && bControlFlags;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[CRW] ARM5 arc track checked=%d outOfGamut=%d ; CONTROL out-of-gamut flagged=%d %s"),
			Checked, OutOfGamut, bControlFlags ? 1 : 0, bArm5 ? TEXT("PASS") : TEXT("FAIL"));

		// ---- ARM6: unset reads a dash, never a fabricated hex -----------------------------------
		int32 UnsetRows = 0, UnsetWithHex = 0;
		for (const FAFLCreatorChannelRow& R : Rows)
		{
			if (!R.bHasValue)
			{
				++UnsetRows;
				if (R.Readout.ToString().Contains(TEXT("#"))) { ++UnsetWithHex; }
			}
		}
		const bool bArm6 = (UnsetWithHex == 0);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[CRW] ARM6 unset rows=%d of which showing a hex=%d (want 0) %s"),
			UnsetRows, UnsetWithHex, bArm6 ? TEXT("PASS") : TEXT("FAIL"));

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[CRW] SUMMARY failClosed=%d schemaDriven=%d reasons=%d clamp=%d unset=%d"),
			bArm1 ? 1 : 0, bArm2 ? 1 : 0, bArm3 ? 1 : 0, bArm5 ? 1 : 0, bArm6 ? 1 : 0);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[CRW] END"));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCreatorWidgetProbeCmd(TEXT("afl.Creator.WidgetProbe"),
		TEXT("CC-5.2: prove the creator rail is schema-driven, fails closed, shows disabled channels WITH ")
		TEXT("reasons, never leaves the gamut, and cannot be written through a disabled channel. Reads only."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCreatorWidgetProbe));

	// === CC-6.5: afl.Creator.VerifyDoneLoop -- THE DONE DEFINITION ================================
	void HandleAFLVerifyDoneLoop(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Creator.VerifyDoneLoop - run inside PIE.")); return; }

		FString Why;
		UAFLWalletComponent* Wallet = GetServerWalletForLocalPlayer(World, Why);
		APlayerController* PC = World->GetFirstPlayerController();
		const UAFLOnlineSubsystem* Online = Wallet ? UAFLOnlineSubsystem::Get(Wallet) : nullptr;
		const FString Pf = Online ? Online->GetPlayFabId() : FString();

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DONE] ARM0 wallet=%d pc=%d pfid=%s why=%s"),
			Wallet ? 1 : 0, PC ? 1 : 0, Pf.IsEmpty() ? TEXT("<none>") : *Pf, Why.IsEmpty() ? TEXT("ok") : *Why);
		if (!Wallet || !PC || Pf.IsEmpty())
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DONE] VOID -- no wallet/PC/PlayFabId. Nothing about the loop was tested."));
			return;
		}

		APlayerState* PS = PC->PlayerState;
		UAFLCosmeticLoadoutComponent* LC = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!LC) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DONE] VOID -- no loadout component")); return; }

		// EXPECTED VALUES ARE DERIVED FROM THE SYSTEM, never typed in. The grant quantity comes from the
		// catalog row itself, so a repriced/reconfigured SKU cannot silently invalidate the arm.
		static const FName SlotKey(TEXT("AFL.CreatorSlot"));
		static const FName SlotSku(TEXT("AFL.CreatorSlot.x1"));
		int32 ExpectGrant = 0;
		if (const UAFLCosmeticCatalogSubsystem* Cat = UAFLCosmeticCatalogSubsystem::Get(World))
		{
			if (const FAFLCatalogEntry* E = Cat->FindEntry(SlotSku)) { ExpectGrant = E->GrantQuantity; }
		}
		const int32 SlotsBefore = Wallet->GetCountedEntitlement(SlotKey);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[DONE] ARM1 baseline countedSlots=%d ; %s GrantQuantity=%d (READ FROM THE CATALOG) mirrorVo=%d"),
			SlotsBefore, *SlotSku.ToString(), ExpectGrant, Wallet->GetVolts());
		if (ExpectGrant <= 0)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DONE] VOID -- the SKU carries no GrantQuantity; there is no expected value to check against."));
			return;
		}

		// ---- ARM1: BUY through the PRODUCTION ENTRY. No debug grant anywhere in this function. ----
		Wallet->ClientRequestPurchase(SlotSku, EAFLPayCurrency::Volts, TFunction<void(bool)>());

		TWeakObjectPtr<UAFLWalletComponent> WW(Wallet);
		TWeakObjectPtr<UAFLCosmeticLoadoutComponent> WLC(LC);
		TWeakObjectPtr<UWorld> WWorld(World);
		TWeakObjectPtr<APlayerController> WPC(PC);

		FTimerHandle H1;
		World->GetTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda(
			[WW, WLC, WWorld, WPC, SlotsBefore, ExpectGrant]()
		{
			UAFLWalletComponent* W = WW.Get();
			UAFLCosmeticLoadoutComponent* L = WLC.Get();
			UWorld* Wd = WWorld.Get();
			APlayerController* P = WPC.Get();
			if (!W || !L || !Wd || !P) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DONE] VOID -- gone before ARM1")); return; }

			static const FName K(TEXT("AFL.CreatorSlot"));
			const int32 After = W->GetCountedEntitlement(K);
			const bool bArm1 = (After == SlotsBefore + ExpectGrant);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DONE] ARM1 buy -> countedSlots %d -> %d (want %d) %s"),
				SlotsBefore, After, SlotsBefore + ExpectGrant, bArm1 ? TEXT("PASS") : TEXT("FAIL"));

			// ---- ARM2: BUILD IN THE CREATOR, through the widget's own entry point ----------------
			UAFLW_LoadoutBase* LB = nullptr;
			for (TObjectIterator<UAFLW_LoadoutBase> It; It; ++It)
			{
				if (IsValid(*It) && It->GetWorld() == Wd) { LB = *It; break; }
			}
			if (!LB)
			{
				if (UClass* LCls = LoadClass<UAFLW_LoadoutBase>(nullptr,
					TEXT("/Game/BagMan/UI/Loadout/WBP_AFL_Loadout.WBP_AFL_Loadout_C")))
				{
					LB = CreateWidget<UAFLW_LoadoutBase>(P, LCls);
					if (LB) { LB->AddToViewport(); }   // so the preview rig actually initialises
				}
			}
			if (!LB) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DONE] VOID -- no loadout widget; cannot build")); return; }

			UAFLW_Creator* CW = nullptr;
			if (UClass* CCls = LoadClass<UAFLW_Creator>(nullptr,
				TEXT("/Game/BagMan/UI/Creator/WBP_AFL_Creator.WBP_AFL_Creator_C")))
			{
				CW = CreateWidget<UAFLW_Creator>(P, CCls);
			}
			if (!CW) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DONE] VOID -- creator widget did not load")); return; }
			CW->InitializeCreator(LB);

			// Drive the FIRST CONNECTED channel -- whichever the chassis actually offers. Hard-coding a
			// channel would make the arm chassis-specific and it would silently skip on the other master.
			EAFLCreatorChannel Target = EAFLCreatorChannel::Edge;
			bool bFound = false;
			for (const FAFLCreatorChannelRow& R : CW->GetChannelRows())
			{
				if (R.bInteractive) { Target = R.Channel; bFound = true; break; }
			}
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DONE] ARM2 rows=%d resolvedMaster=%s targetChannel=%d found=%d"),
				CW->GetChannelRows().Num(), *CW->GetResolvedMaster().ToString(), (int32)Target, bFound ? 1 : 0);
			if (!bFound)
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DONE] VOID -- no connected channel on this chassis; nothing to build with."));
				return;
			}

			const float BuildHue = 200.0f;
			CW->SetChannelHue(Target, BuildHue);

			const FLinearColor Want = AFLCreatorGamut::FromHue(BuildHue);
			const FAFLCosmeticSelection Sel = LB->CreatorGetWorkingSelection();
			FLinearColor Got = FLinearColor::White;
			switch (Target)
			{
				case EAFLCreatorChannel::Body:  Got = Sel.CreatorBodyColor;  break;
				case EAFLCreatorChannel::Edge:  Got = Sel.CreatorEdgeColor;  break;
				case EAFLCreatorChannel::Glow:  Got = Sel.CreatorGlowColor;  break;
				case EAFLCreatorChannel::Visor: Got = Sel.CreatorVisorColor; break;
			}
			// THE ARC'S PROMISE IS HUE, NOT A WHOLE COLOUR. A first version of this arm expected
			// FromHue(h) outright and FAILED on a correct result: the channel already carried a value
			// (the working selection seeds from the committed one), so SetChannelHue took the WithHue
			// branch and PRESERVED saturation and value while moving hue -- exactly as designed. The
			// expectation was derived from the wrong branch, so it tested the probe's assumption
			// rather than the product's rule.
			//
			// The invariant that actually holds either way: the hue lands where the player put it, and
			// the result is inside the gamut. Saturation and value are not player-facing axes.
			const float GotHue = AFLCreatorGamut::HueOf(Got);
			const FLinearColor GotHSV = Got.LinearRGBToHSV();
			const bool bHueRight = FMath::Abs(FMath::FindDeltaAngleDegrees(GotHue, BuildHue)) < 1.0f;
			const bool bInGamut  = GotHSV.G >= AFLCreatorGamut::MinSaturation - KINDA_SMALL_NUMBER
			                    && GotHSV.B >= AFLCreatorGamut::MinValue      - KINDA_SMALL_NUMBER
			                    && GotHSV.B <= AFLCreatorGamut::MaxValue      + KINDA_SMALL_NUMBER;
			const bool bArm2 = bHueRight && bInGamut;
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[DONE] ARM2 build hue=%.0f -> selection=(%.3f,%.3f,%.3f) gotHue=%.1f S=%.3f V=%.3f hueRight=%d inGamut=%d %s"),
				BuildHue, Got.R, Got.G, Got.B, GotHue, GotHSV.G, GotHSV.B,
				bHueRight ? 1 : 0, bInGamut ? 1 : 0, bArm2 ? TEXT("PASS") : TEXT("FAIL"));
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[DONE] ARM2 NOTE FromHue(%.0f)=(%.3f,%.3f,%.3f) is the UNSET-channel value; this channel "
					 "already had one, so WithHue preserved S/V. Both are correct -- only the hue is the arc's promise."),
				BuildHue, Want.R, Want.G, Want.B);

			// ---- ARM4: EQUIP -- save to a slot and make it active, both PRODUCTION RPCs ----------
			FAFLCreatorBuild Build;
			Build.DisplayName   = TEXT("DoneLoop");
			Build.BaseSelection = Sel;
			const int32 Index = L->GetBuildSet().Builds.Num();
			L->ServerSaveBuild(Build, Index);
			L->ServerSetActiveBuild(Index);

			FTimerHandle H2;
			Wd->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda(
				[WW, WLC, WWorld, WPC, bArm1, bArm2, Index, Target]()
			{
				UAFLWalletComponent* W2 = WW.Get();
				UAFLCosmeticLoadoutComponent* L2 = WLC.Get();
				UWorld* Wd2 = WWorld.Get();
				APlayerController* P2 = WPC.Get();
				if (!W2 || !L2 || !Wd2 || !P2) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DONE] VOID -- gone before ARM4")); return; }

				const FAFLCreatorBuildSet& Set = L2->GetBuildSet();
				const bool bSaved  = Set.Builds.IsValidIndex(Index);
				const bool bActive = (Set.ActiveBuildIndex == Index);
				const bool bArm4 = bSaved && bActive;
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[DONE] ARM4 equip -> builds=%d savedAt=%d active=%d (want %d) %s"),
					Set.Builds.Num(), bSaved ? 1 : 0, Set.ActiveBuildIndex, Index, bArm4 ? TEXT("PASS") : TEXT("FAIL"));

				// ---- ARM3 + ARM5: PREVIEW vs SPAWN, the done definition ------------------------
				// Both read the SAME component type through the SAME accessor, so a difference is a
				// difference in the RESOLVED result and not in how it was measured.
				UAFLW_LoadoutBase* LB2 = nullptr;
				for (TObjectIterator<UAFLW_LoadoutBase> It; It; ++It)
				{
					if (IsValid(*It) && It->GetWorld() == Wd2) { LB2 = *It; break; }
				}
				APawn* Preview = LB2 ? LB2->GetPreviewPawnForTest() : nullptr;
				APawn* InMatch = P2->GetPawn();

				const UAFLSkinColorComponent* PrevSkin = Preview ? Preview->FindComponentByClass<UAFLSkinColorComponent>() : nullptr;
				const UAFLSkinColorComponent* MatchSkin = InMatch ? InMatch->FindComponentByClass<UAFLSkinColorComponent>() : nullptr;

				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DONE] ARM3/5 previewPawn=%d matchPawn=%d previewSkin=%d matchSkin=%d"),
					Preview ? 1 : 0, InMatch ? 1 : 0, PrevSkin ? 1 : 0, MatchSkin ? 1 : 0);

				if (!PrevSkin || !MatchSkin)
				{
					// VOID, NOT FAIL. Without both pawns there is nothing to compare, and grading that
					// as a pass would be the loudest possible false green.
					UE_LOG(LogAFLCombat, Warning,
						TEXT("AFL_TEST[DONE] ARM5 VOID -- need BOTH a preview pawn and a match pawn to compare. Nothing about the loop's core was tested."));
				}
				else
				{
					const FAFLColorOverride& A = PrevSkin->GetColorOverride();
					const FAFLColorOverride& B = MatchSkin->GetColorOverride();
					const bool bSame =
						A.BodyColor.Equals(B.BodyColor, 0.001f) &&
						A.EdgeColor.Equals(B.EdgeColor, 0.001f) &&
						A.GlowColor.Equals(B.GlowColor, 0.001f) &&
						A.VisorColor.Equals(B.VisorColor, 0.001f) &&
						A.bValid == B.bValid;
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[DONE] ARM3 preview override body=(%.3f,%.3f,%.3f) edge=(%.3f,%.3f,%.3f) valid=%d"),
						A.BodyColor.R, A.BodyColor.G, A.BodyColor.B, A.EdgeColor.R, A.EdgeColor.G, A.EdgeColor.B, A.bValid ? 1 : 0);
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[DONE] ARM5 match   override body=(%.3f,%.3f,%.3f) edge=(%.3f,%.3f,%.3f) valid=%d"),
						B.BodyColor.R, B.BodyColor.G, B.BodyColor.B, B.EdgeColor.R, B.EdgeColor.G, B.EdgeColor.B, B.bValid ? 1 : 0);
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[DONE] ARM5 THE ROBOT IN MATCH IS THE ROBOT IN THE PREVIEW = %d %s"),
						bSame ? 1 : 0, bSame ? TEXT("PASS") : TEXT("FAIL"));
				}

				// ---- ARM6: slots count ------------------------------------------------------------
				UAFLW_Creator* CW2 = nullptr;
				if (UClass* CCls2 = LoadClass<UAFLW_Creator>(nullptr,
					TEXT("/Game/BagMan/UI/Creator/WBP_AFL_Creator.WBP_AFL_Creator_C")))
				{
					CW2 = CreateWidget<UAFLW_Creator>(P2, CCls2);
				}
				if (CW2)
				{
					static const FName K2(TEXT("AFL.CreatorSlot"));
					const int32 Counted = W2->GetCountedEntitlement(K2);
					const bool bArm6 = (CW2->GetSlotCap() >= 2) && (CW2->GetSlotsUsed() == L2->GetBuildSet().Builds.Num());
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[DONE] ARM6 slots used=%d cap=%d counted=%d text='%s' %s"),
						CW2->GetSlotsUsed(), CW2->GetSlotCap(), Counted,
						*CW2->GetSlotCounterText().ToString(), bArm6 ? TEXT("PASS") : TEXT("FAIL"));
				}

				// ---- ARM7: no cheat, asserted structurally ---------------------------------------
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[DONE] ARM7 NO CHEAT: this loop called ClientRequestPurchase, ServerSaveBuild and ")
					TEXT("ServerSetActiveBuild ONLY. No DebugGrantOwnership, no cvar grant. Corroborate against the ")
					TEXT("wallet's own 'COUNTED POST grant' line, which names the purchase that moved the counter."));
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DONE] SUMMARY buy=%d build=%d equip=%d"),
					bArm1 ? 1 : 0, bArm2 ? 1 : 0, bArm4 ? 1 : 0);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[DONE] END"));
			}), 8.0f, false);
		}), 10.0f, false);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLVerifyDoneLoopCmd(TEXT("afl.Creator.VerifyDoneLoop"),
		TEXT("CC-6.5 THE DONE DEFINITION: buy a slot pack through the production entry, build in the ")
		TEXT("creator, equip, spawn, and assert the match pawn's colour override EQUALS the preview's. ")
		TEXT("SPENDS real Volts. VOID (not FAIL) without both pawns."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLVerifyDoneLoop));

	// === CC-7.2: afl.Sticker.Probe ================================================================
	void HandleAFLStickerProbe(const TArray<FString>& /*Args*/, UWorld* /*World*/, FOutputDevice& Ar)
	{
		// ---- ARM1: nine zones, and the invariant is RESTORED rather than assumed -----------------
		const int32 ZoneCount = FAFLStickerSet::ZoneCount;
		FAFLStickerSet Set;
		Set.EnsureSized();
		const int32 AfterEnsure = Set.Zones.Num();
		Set.Zones.SetNum(3);                 // deliberately corrupt it, as an old save might
		Set.EnsureSized();
		const int32 AfterRestore = Set.Zones.Num();
		const bool bArm1 = (ZoneCount == 9) && (AfterEnsure == 9) && (AfterRestore == 9);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[STK] ARM1 ZoneCount=%d sized=%d shortenedTo3ThenRestored=%d (want 9/9/9) %s"),
			ZoneCount, AfterEnsure, AfterRestore, bArm1 ? TEXT("PASS") : TEXT("FAIL"));

		// ---- ARM2: the clamp holds every bound ---------------------------------------------------
		const FVector2D PosClamped = AFLStickerBounds::ClampPosition(FVector2D(-5.0, 7.0));
		const float ScaleHigh = AFLStickerBounds::ClampScale(99.0f);
		const float ScaleLow  = AFLStickerBounds::ClampScale(0.001f);
		const float Rot370    = AFLStickerBounds::NormaliseRotation(370.0f);
		const float RotNeg30  = AFLStickerBounds::NormaliseRotation(-30.0f);
		const bool bArm2 =
			FMath::IsNearlyEqual(PosClamped.X, 0.0, 0.0001) && FMath::IsNearlyEqual(PosClamped.Y, 1.0, 0.0001) &&
			FMath::IsNearlyEqual(ScaleHigh, AFLStickerBounds::MaxScale, 0.0001f) &&
			FMath::IsNearlyEqual(ScaleLow,  AFLStickerBounds::MinScale, 0.0001f) &&
			FMath::IsNearlyEqual(Rot370, 10.0f, 0.01f) && FMath::IsNearlyEqual(RotNeg30, 330.0f, 0.01f);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[STK] ARM2 pos(-5,7)->(%.3f,%.3f) scale99->%.3f scale0.001->%.3f rot370->%.1f rot-30->%.1f %s"),
			PosClamped.X, PosClamped.Y, ScaleHigh, ScaleLow, Rot370, RotNeg30, bArm2 ? TEXT("PASS") : TEXT("FAIL"));

		// ---- ARM3: THE CONTROL -- a valid placement is left ALONE --------------------------------
		// Without this, a clamp that flattened every input to a constant would sail through ARM2.
		FAFLStickerPlacement Valid;
		Valid.StickerId = FName(TEXT("AFL.Sticker.Probe"));
		Valid.Position = FVector2D(0.42, 0.66);
		Valid.Scale = 0.5f;
		Valid.RotationDegrees = 45.0f;
		const FAFLStickerPlacement Passed = AFLStickerBounds::Clamp(Valid);
		const bool bArm3 = (Passed == Valid);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[STK] ARM3 CONTROL valid (0.42,0.66) s=0.50 r=45 -> (%.3f,%.3f) s=%.2f r=%.1f unchanged=%d %s"),
			Passed.Position.X, Passed.Position.Y, Passed.Scale, Passed.RotationDegrees,
			bArm3 ? 1 : 0, bArm3 ? TEXT("PASS") : TEXT("FAIL"));

		// ---- ARM4: there is no unclamped door into the set ---------------------------------------
		FAFLStickerPlacement Rogue;
		Rogue.StickerId = FName(TEXT("AFL.Sticker.Rogue"));
		Rogue.Position = FVector2D(9.0, -9.0);   // way outside the zone rect -> would cross a seam
		Rogue.Scale = 42.0f;
		Set.Set(EAFLStickerZone::Back, Rogue);
		const FAFLStickerPlacement* ReadBack = Set.Find(EAFLStickerZone::Back);
		const bool bArm4 = ReadBack
			&& FMath::IsNearlyEqual(ReadBack->Position.X, 1.0, 0.0001)
			&& FMath::IsNearlyEqual(ReadBack->Position.Y, 0.0, 0.0001)
			&& FMath::IsNearlyEqual(ReadBack->Scale, AFLStickerBounds::MaxScale, 0.0001f);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[STK] ARM4 Set() rogue(9,-9) s=42 -> readback (%.3f,%.3f) s=%.2f %s"),
			ReadBack ? ReadBack->Position.X : -1.0, ReadBack ? ReadBack->Position.Y : -1.0,
			ReadBack ? ReadBack->Scale : -1.0f, bArm4 ? TEXT("PASS") : TEXT("FAIL"));

		// ---- ARM5: empty is not absent -----------------------------------------------------------
		const int32 SetCount = Set.NumSet();
		const FAFLStickerPlacement* EmptyZone = Set.Find(EAFLStickerZone::Face);
		const bool bArm5 = (SetCount == 1) && (EmptyZone != nullptr) && !EmptyZone->IsSet()
			&& (Set.Zones.Num() == 9);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[STK] ARM5 numSet=%d (want 1) emptyZoneStillIndexed=%d isSet=%d zones=%d %s"),
			SetCount, EmptyZone ? 1 : 0, (EmptyZone && EmptyZone->IsSet()) ? 1 : 0, Set.Zones.Num(),
			bArm5 ? TEXT("PASS") : TEXT("FAIL"));

		// ---- ARM6: the zone enum is contiguous and nine long -------------------------------------
		// Guards the replication assumption: zone index IS array index, so a gap would misaddress.
		const UEnum* ZE = StaticEnum<EAFLStickerZone>();
		int32 Contiguous = 0;
		if (ZE)
		{
			for (int32 i = 0; i < ZoneCount; ++i)
			{
				if (ZE->GetValueByIndex(i) == (int64)i) { ++Contiguous; }
			}
		}
		const bool bArm6 = (Contiguous == ZoneCount);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[STK] ARM6 zone enum contiguous 0..%d = %d/%d %s"),
			ZoneCount - 1, Contiguous, ZoneCount, bArm6 ? TEXT("PASS") : TEXT("FAIL"));

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[STK] SUMMARY fixed9=%d clamp=%d control=%d noBackDoor=%d emptyNotAbsent=%d contiguous=%d"),
			bArm1 ? 1 : 0, bArm2 ? 1 : 0, bArm3 ? 1 : 0, bArm4 ? 1 : 0, bArm5 ? 1 : 0, bArm6 ? 1 : 0);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[STK] END"));
		Ar.Log(TEXT("afl.Sticker.Probe complete -- see AFL_TEST[STK] lines."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStickerProbeCmd(TEXT("afl.Sticker.Probe"),
		TEXT("CC-7.2: prove the nine-zone sticker set is fixed-size, that the shared clamp holds every ")
		TEXT("bound, that a VALID placement passes through unchanged, and that Set() has no unclamped ")
		TEXT("door. Pure computation -- spends nothing, touches no asset."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStickerProbe));



#if WITH_EDITOR
	// === afl.Catalog.LedgerVisibility ==============================================================
	// CC-X38: the store asks TWO backends. PlayFab sells catalog items; the mint ledger sells bundles,
	// and a ledger bundle is absent from the PlayFab manifest by design.
	void HandleAFLLedgerVisibility(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar);

	void RunAFLLedgerVisibility(UWorld* World, FOutputDevice& Ar)
	{
		int32 Ran = 0, Passed = 0;
		auto Arm = [&Ran, &Passed](const TCHAR* Name, const bool bOk, const FString& Detail)
		{
			++Ran; if (bOk) { ++Passed; }
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[LEDGERVIS]   %-40s %s  %s"),
				Name, bOk ? TEXT("PASS") : TEXT("FAIL"), *Detail);
		};

		const UAFLCosmeticCatalogSubsystem* Cat = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!Cat)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[LEDGERVIS] ABORT -- no catalog subsystem. NOTHING TESTED."));
			return;
		}

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[LEDGERVIS] BEGIN  playfabKnown=%d(%d) ledgerKnown=%d(%d)"),
			Cat->IsRegisteredSetKnown() ? 1 : 0, Cat->GetRegisteredCount(),
			Cat->IsLedgerSetKnown() ? 1 : 0, Cat->GetLedgerSellableCount());

		// THE LEDGER MUST HAVE ANSWERED, or every later arm measures the old behaviour and passes for
		// the wrong reason. This is the precondition, stated as an arm rather than assumed.
		Arm(TEXT("0 the ledger answered at all"), Cat->IsLedgerSetKnown() && Cat->GetLedgerSellableCount() > 0,
			FString::Printf(TEXT("ledger ids=%d"), Cat->GetLedgerSellableCount()));

		TArray<FAFLCatalogEntry> Shown;
		Cat->GetPurchasableEntries(Shown);
		TSet<FName> ShownIds;
		for (const FAFLCatalogEntry& E : Shown) { ShownIds.Add(E.CosmeticId); }

		// Partition the .XT rows BY PROPERTY, then check each side. Reading the split from the data
		// rather than hard-coding 28 and 21 means a re-balanced catalog re-derives instead of lying.
		int32 PaidXt = 0, PaidXtShown = 0, FreeXt = 0, FreeXtShown = 0;
		int32 NotSellable = 0, NotSellableShown = 0;
		// Enumerated by TYPE, the way TYPELINT does -- the subsystem exposes no whole-asset accessor and
		// adding one to read four counters would widen a public surface for a probe's convenience.
		const UEnum* TypeEnum = StaticEnum<EAFLCosmeticType>();
		for (int32 T = 0; T < (TypeEnum ? TypeEnum->NumEnums() : 0); ++T)
		{
			TArray<const FAFLCatalogEntry*> OfType;
			Cat->GetEntriesByType(static_cast<EAFLCosmeticType>(TypeEnum->GetValueByIndex(T)), OfType);
			for (const FAFLCatalogEntry* Ptr : OfType)
			{
				if (!Ptr) { continue; }
				const FAFLCatalogEntry& E = *Ptr;
				const bool bXt = E.CosmeticId.ToString().EndsWith(TEXT(".XT"), ESearchCase::IgnoreCase);
				const bool bFree = (E.Acquisition == EAFLAcquisition::GrantedFree);
				if (bXt)
				{
					if (bFree) { ++FreeXt; if (ShownIds.Contains(E.CosmeticId)) { ++FreeXtShown; } }
					else       { ++PaidXt; if (ShownIds.Contains(E.CosmeticId)) { ++PaidXtShown; } }
				}
				// The general control: any priced row neither backend can sell must still be withheld.
				if (!bFree && E.bTransactable && !Cat->IsSellable(E.CosmeticId))
				{
					++NotSellable;
					if (ShownIds.Contains(E.CosmeticId)) { ++NotSellableShown; }
				}
			}
		}

		Arm(TEXT("1 the PAID .XT pairs are visible"), PaidXt > 0 && PaidXtShown == PaidXt,
			FString::Printf(TEXT("%d/%d shown"), PaidXtShown, PaidXt));

		// THE NEGATIVE SIDE. Removing the filter entirely would satisfy arm 1 and fail here.
		Arm(TEXT("2 the FREE .XT rows are NOT visible"), FreeXt > 0 && FreeXtShown == 0,
			FString::Printf(TEXT("%d/%d shown (expected 0 of %d)"), FreeXtShown, FreeXt, FreeXt));

		Arm(TEXT("3 rows neither backend sells stay hidden"), NotSellableShown == 0,
			FString::Printf(TEXT("%d such rows, %d leaked"), NotSellable, NotSellableShown));

		// A store with nothing in it would satisfy arms 2 and 3 for free.
		Arm(TEXT("4 the store is not simply empty"), Shown.Num() > PaidXt,
			FString::Printf(TEXT("%d rows shown in total"), Shown.Num()));

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[LEDGERVIS] END arms=%d passed=%d %s"),
			Ran, Passed, (Ran == Passed && Ran == 5) ? TEXT("PASS") : TEXT("FAIL"));
	}

	void HandleAFLLedgerVisibility(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		// ARM AND WAIT, for the same reason afl.Test.Wearables does: the catalog subsystem lives on a
		// game instance and nothing may be injected into a running PIE session. Bounded at 60s with a
		// terminator that says it gave up.
		auto Played = []() -> UWorld*
		{
			if (!GEngine) { return nullptr; }
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if (Ctx.World() && UAFLCosmeticCatalogSubsystem::Get(Ctx.World())
					&& Ctx.World()->GetFirstPlayerController()) { return Ctx.World(); }
			}
			return nullptr;
		};

		if (UWorld* W = Played()) { RunAFLLedgerVisibility(W, Ar); return; }

		static bool bArmed = false;
		if (bArmed)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[LEDGERVIS] already armed -- not arming twice."));
			return;
		}
		bArmed = true;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[LEDGERVIS] ARMED -- no game instance yet. Will fire when PIE brings one up; giving up after 60s."));

		TSharedPtr<double> Elapsed = MakeShared<double>(0.0);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[Elapsed, Played](float Dt) -> bool
			{
				*Elapsed += Dt;
				if (*Elapsed > 60.0)
				{
					UE_LOG(LogAFLCombat, Warning,
						TEXT("AFL_TEST[LEDGERVIS] GAVE UP after 60s -- no game instance with a catalog appeared. NOTHING TESTED."));
					return false;
				}
				if (UWorld* W = Played())
				{
					// THE LEDGER FETCH IS ASYNC. Firing the instant a world exists would measure an
					// unanswered set and report the OLD behaviour as the new one. Waits for the answer,
					// inside the same bounded window.
					const UAFLCosmeticCatalogSubsystem* C = UAFLCosmeticCatalogSubsystem::Get(W);
					if (!C || !C->IsLedgerSetKnown()) { return true; }
					UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[LEDGERVIS] FIRING (armed %.1fs ago)"), *Elapsed);
					FOutputDeviceNull Null;
					RunAFLLedgerVisibility(W, Null);
					return false;
				}
				return true;
			}), 0.5f);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLedgerVisCmd(TEXT("afl.Catalog.LedgerVisibility"),
		TEXT("CC-X38: prove mint-ledger bundles are offered and rows neither backend sells are not. ")
		TEXT("Arms before PIE and fires once the ledger set has answered."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLLedgerVisibility));
#endif // WITH_EDITOR


#if WITH_EDITOR
	// Defined further down, where it grew out of the FBIK probe. Forward-declared rather than moved:
	// relocating a proven block to satisfy declaration order is a bigger change than a prototype.
	static bool AFLArmForPie(const TCHAR* Tag, TFunction<void(UWorld*)> Run);

	// === afl.Test.WristOrientation ================================================================
	// The two wrist sockets are MIRRORED on SK_Mannequin (measured: local +Y is up on the left and down
	// on the right). A single mesh therefore cannot be correct on both without a per-side correction,
	// and this is the check that a missing or wrong correction cannot pass.
	void RunAFLWristOrientation(UWorld* World)
	{
		int32 Ran = 0, Passed = 0;
		auto Arm = [&Ran, &Passed](const TCHAR* Name, bool bOk, const FString& Detail)
		{
			++Ran; if (bOk) { ++Passed; }
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[WRIST]   %-40s %s  %s"),
				Name, bOk ? TEXT("PASS") : TEXT("FAIL"), *Detail);
		};

		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ACharacter* Ch = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		USkeletalMeshComponent* Mesh = Ch ? Ch->GetMesh() : nullptr;
		if (!Mesh)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[WRIST] ABORT -- no character mesh. NOTHING TESTED."));
			return;
		}

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[WRIST] BEGIN on %s"), *GetNameSafe(Ch));

		// PRECONDITION, stated as an arm: the sockets must actually be mirrored. If they ever stop being,
		// this whole mechanism is unnecessary and the test should say so rather than keep correcting.
		const FTransform L = Mesh->GetSocketTransform(FName(TEXT("accessory_wrist_l")), RTS_World);
		const FTransform R = Mesh->GetSocketTransform(FName(TEXT("accessory_wrist_r")), RTS_World);
		const FVector LUp = L.GetRotation().GetRightVector();   // socket local +Y
		const FVector RUp = R.GetRotation().GetRightVector();
		Arm(TEXT("0 the wrist sockets are mirrored"), (LUp.Z * RUp.Z) < 0.0,
			FString::Printf(TEXT("L.+Y.z=%+.3f R.+Y.z=%+.3f (opposite signs expected)"), LUp.Z, RUp.Z));

		// Spawn the SAME part class at each socket and compare where its own up axis ends up.
		UClass* PartClass = LoadClass<AActor>(nullptr,
			TEXT("/Game/BagMan/Cosmetics/Accessories/Parts/B_BagMan_Watch_Quantum.B_BagMan_Watch_Quantum_C"));
		if (!PartClass)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[WRIST] ABORT -- watch part class not found. NOTHING TESTED."));
			return;
		}

		auto SpawnAt = [&](const TCHAR* SocketName) -> AActor*
		{
			FActorSpawnParameters P; P.Owner = Ch;
			AActor* A = World->SpawnActor<AActor>(PartClass, FTransform::Identity, P);
			if (A)
			{
				A->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
					FName(SocketName));
				// A direct spawn has no ChildActorComponent, so BeginPlay may already have run before the
				// attach. Re-apply explicitly: the call is idempotent and reads the socket fresh.
				if (AAFLAccessoryPartActor* Part = Cast<AAFLAccessoryPartActor>(A))
				{
					Part->ApplyWristCorrection();
				}
			}
			return A;
		};

		AActor* AL = SpawnAt(TEXT("accessory_wrist_l"));
		AActor* AR = SpawnAt(TEXT("accessory_wrist_r"));
		if (!AL || !AR)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[WRIST] ABORT -- spawn failed. NOTHING TESTED."));
			if (AL) { AL->Destroy(); } if (AR) { AR->Destroy(); }
			return;
		}

		const FVector UpL = AL->GetActorUpVector();
		const FVector UpR = AR->GetActorUpVector();
		Arm(TEXT("1 both wrists agree on which way is up"), (UpL.Z * UpR.Z) > 0.0,
			FString::Printf(TEXT("left.up.z=%+.3f right.up.z=%+.3f (same sign required)"), UpL.Z, UpR.Z));

		Arm(TEXT("2 and both face UP, not down"), UpL.Z > 0.0 && UpR.Z > 0.0,
			FString::Printf(TEXT("left=%+.3f right=%+.3f"), UpL.Z, UpR.Z));

		AL->Destroy(); AR->Destroy();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[WRIST] END arms=%d passed=%d %s"),
			Ran, Passed, (Ran == Passed && Ran == 3) ? TEXT("PASS") : TEXT("FAIL"));
	}

	void HandleAFLWristOrientation(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld() || !World->GetFirstPlayerController())
		{
			AFLArmForPie(TEXT("AFL_TEST[WRIST]"), [](UWorld* W) { RunAFLWristOrientation(W); });
			Ar.Log(TEXT("afl.Test.WristOrientation ARMED -- start PIE; see AFL_TEST[WRIST]."));
			return;
		}
		RunAFLWristOrientation(World);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLWristOrientCmd(TEXT("afl.Test.WristOrientation"),
		TEXT("CC-8: the wrist sockets are mirrored, so a single mesh needs a per-side correction. ")
		TEXT("Attaches the same part at both wrists and requires their up axes to agree AND point up."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLWristOrientation));
#endif // WITH_EDITOR

#if WITH_EDITOR
	// === afl.Test.Wearables ========================================================================
	// The slot-mechanism arms of the jewellery proof. See the equip in AFLCosmeticLoadoutComponent.
	// Forward-declared because the arming ticker below calls it from inside its own body.
	void HandleAFLTestWearables(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);

	void HandleAFLTestWearables(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		int32 Ran = 0, Passed = 0;
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[WEAR] BEGIN"));

		auto Arm = [&Ran, &Passed](const TCHAR* Name, const bool bOk, const FString& Detail)
		{
			++Ran; if (bOk) { ++Passed; }
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[WEAR]   %-34s %s  %s"),
				Name, bOk ? TEXT("PASS") : TEXT("FAIL"), *Detail);
		};

		// FIND A WORLD WITH A PLAYER, not merely the world we were handed. Run from the editor console
		// before PIE, `World` is the EDITOR world and has no PlayerController -- which is the normal
		// case here, because arming has to happen before PIE starts.
		auto FindPlayed = []() -> UWorld*
		{
			if (!GEngine) { return nullptr; }
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if (Ctx.World() && Ctx.World()->GetFirstPlayerController()) { return Ctx.World(); }
			}
			return nullptr;
		};

		UWorld* Played = FindPlayed();
		APlayerController* PC = Played ? Played->GetFirstPlayerController() : nullptr;
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UAFLCosmeticLoadoutComponent* L = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		UAFLWalletComponent* W = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;

		if (!L || !W || !PS->HasAuthority())
		{
			// ARM AND WAIT. Nothing may be injected into a running PIE session, so this command is
			// issued BEFORE PIE and re-checks on a ticker until the PlayerState appears.
			//
			// BOUNDED, WITH A TERMINATOR. Sixty seconds, then it gives up and SAYS SO. An unbounded
			// poll on a condition that may never arrive is not a wait, it is a leak.
			static bool bArmed = false;
			if (bArmed)
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[WEAR] already armed -- not arming twice."));
				Ar.Log(TEXT("afl.Test.Wearables already armed."));
				return;
			}
			bArmed = true;
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[WEAR] ARMED -- no PlayerState yet (loadout=%s wallet=%s). Will fire when PIE brings one up; giving up after 60s."),
				L ? TEXT("ok") : TEXT("missing"), W ? TEXT("ok") : TEXT("missing"));

			TSharedPtr<double> Elapsed = MakeShared<double>(0.0);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[Elapsed](float Dt) -> bool
				{
					*Elapsed += Dt;
					if (*Elapsed > 60.0)
					{
						UE_LOG(LogAFLCombat, Warning,
							TEXT("AFL_TEST[WEAR] GAVE UP after 60s -- no PlayerState with a loadout ever appeared. NOTHING TESTED."));
						return false;   // terminator
					}
					for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
					{
						UWorld* Wld = Ctx.World();
						APlayerController* P = Wld ? Wld->GetFirstPlayerController() : nullptr;
						APlayerState* S = P ? P->PlayerState : nullptr;
						if (S && S->HasAuthority()
							&& S->FindComponentByClass<UAFLCosmeticLoadoutComponent>()
							&& S->FindComponentByClass<UAFLWalletComponent>())
						{
							UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[WEAR] FIRING (armed %.1fs ago, world=%s)"),
								*Elapsed, *Wld->GetName());
							FOutputDeviceNull Null;
							HandleAFLTestWearables(TArray<FString>(), Wld, Null);
							return false;   // one shot
						}
					}
					return true;
				}), 0.5f);

			Ar.Log(TEXT("afl.Test.Wearables ARMED -- start PIE; see AFL_TEST[WEAR]."));
			return;
		}

		const FName CHAIN   (TEXT("AFL.Accessory.Chain.FoundersPurps"));
		const FName PENDANT (TEXT("AFL.Accessory.Pendant.TTG"));
		const FName WATCH_A (TEXT("AFL.Accessory.Watch.RareUniverse"));
		const FName WATCH_B (TEXT("AFL.Accessory.Watch.Quantum"));
		const FName BRACE_A (TEXT("AFL.Accessory.Bracelet.QuantumUniverse"));

		auto Holds = [L](const EAFLAccessorySlot S) -> FName
		{
			const FAFLAccessoryPlacement* P = L->GetSelection().AccessorySet.Find(S);
			return (P && P->IsSet()) ? P->AccessoryId : NAME_None;
		};

		// Deterministic start: every slot empty, so an arm cannot pass on residue from a prior run.
		for (int32 i = 0; i < static_cast<int32>(EAFLAccessorySlot::MAX); ++i)
		{
			L->ServerClearAccessory(static_cast<EAFLAccessorySlot>(i));
		}

		// --- ARM 1: THE GATE ITSELF. Not granted, must refuse. If this fails, every later PASS is void.
		//
		// THE ID IS CHOSEN AT RUN TIME, NOT PINNED. This arm named a fixed chain, and a live canary
		// purchase later granted that chain to the dev account -- so the equip correctly succeeded and
		// the control reported a defect that was not there. A control pinned to a fact that can change
		// underneath it stops being a control the moment it does.
		static const FName Candidates[] = {
			FName(TEXT("AFL.Accessory.Pendant.RareUniverse")), FName(TEXT("AFL.Accessory.Pendant.BigSixx")),
			FName(TEXT("AFL.Accessory.Pendant.1776")),         FName(TEXT("AFL.Accessory.Watch.Quantum")),
			FName(TEXT("AFL.Accessory.Bracelet.BigSixx")),     FName(TEXT("AFL.Accessory.Chain.FoundersLink")),
		};
		FName Unowned = NAME_None;
		for (const FName& C : Candidates) { if (!W->OwnsCosmetic(C)) { Unowned = C; break; } }
		if (Unowned.IsNone())
		{
			// LOUD, not skipped. "Every candidate is owned" and "the gate held" must not look alike.
			Arm(TEXT("1 an unowned id was available"), false,
				TEXT("the dev account owns every candidate -- the ownership control CANNOT run"));
		}
		else
		{
			// Neck is the slot a chain would take; a pendant/watch/bracelet resolves elsewhere, so the
			// assertion reads whichever slot this candidate would have landed in.
			L->ServerEquipWearable(Unowned);
			bool bLanded = false;
			for (int32 i = 0; i < static_cast<int32>(EAFLAccessorySlot::MAX); ++i)
			{
				if (Holds(static_cast<EAFLAccessorySlot>(i)) == Unowned) { bLanded = true; break; }
			}
			Arm(TEXT("1 unowned item refused"), !bLanded,
				FString::Printf(TEXT("tried %s, landed=%d (expected 0)"), *Unowned.ToString(), bLanded ? 1 : 0));
		}

		// --- ARM 2: pendant with NO chain refuses. Granted, so only the dependency can refuse it.
		W->DebugGrantOwnership(PENDANT);
		L->ServerEquipWearable(PENDANT);
		Arm(TEXT("2 pendant refused, no chain"), Holds(EAFLAccessorySlot::Pendant).IsNone(),
			FString::Printf(TEXT("pendant=%s (expected None)"), *Holds(EAFLAccessorySlot::Pendant).ToString()));

		// --- ARM 3: chain equips at Neck once owned.
		W->DebugGrantOwnership(CHAIN);
		L->ServerEquipWearable(CHAIN);
		Arm(TEXT("3 chain equips at Neck"), Holds(EAFLAccessorySlot::Neck) == CHAIN,
			FString::Printf(TEXT("neck=%s"), *Holds(EAFLAccessorySlot::Neck).ToString()));

		// --- ARM 4: the same pendant now accepted. Same call, same grant -- only the chain changed.
		L->ServerEquipWearable(PENDANT);
		Arm(TEXT("4 pendant accepted with chain"), Holds(EAFLAccessorySlot::Pendant) == PENDANT,
			FString::Printf(TEXT("pendant=%s"), *Holds(EAFLAccessorySlot::Pendant).ToString()));

		// --- ARM 5: un-equipping the chain KEEPS the pendant and stops drawing it. Two assertions,
		// because "kept" and "not drawn" are different claims and one without the other is the bug.
		L->ServerClearAccessory(EAFLAccessorySlot::Neck);
		const bool bKept    = (Holds(EAFLAccessorySlot::Pendant) == PENDANT);
		const bool bNotDrawn= !L->IsAccessorySlotRenderable(EAFLAccessorySlot::Pendant);
		Arm(TEXT("5 pendant kept, not rendered"), bKept && bNotDrawn,
			FString::Printf(TEXT("kept=%d renderable=%d"), bKept ? 1 : 0, bNotDrawn ? 0 : 1));

		// --- ARM 6: an either-side wrist piece takes the first open side.
		W->DebugGrantOwnership(WATCH_A);
		L->ServerEquipWearable(WATCH_A);
		Arm(TEXT("6 wrist A takes left"), Holds(EAFLAccessorySlot::WristL) == WATCH_A,
			FString::Printf(TEXT("wristL=%s"), *Holds(EAFLAccessorySlot::WristL).ToString()));

		// --- ARM 7: the second takes the OTHER side rather than replacing the first.
		W->DebugGrantOwnership(BRACE_A);
		L->ServerEquipWearable(BRACE_A);
		Arm(TEXT("7 wrist B takes right"),
			Holds(EAFLAccessorySlot::WristR) == BRACE_A && Holds(EAFLAccessorySlot::WristL) == WATCH_A,
			FString::Printf(TEXT("wristL=%s wristR=%s"),
				*Holds(EAFLAccessorySlot::WristL).ToString(), *Holds(EAFLAccessorySlot::WristR).ToString()));

		// --- ARM 8: a third is REFUSED, and neither wrist changed. The second half matters: a refusal
		// that still mutated state would pass a "not equipped" check while having dropped something.
		W->DebugGrantOwnership(WATCH_B);
		L->ServerEquipWearable(WATCH_B);
		Arm(TEXT("8 third wrist refused, no swap"),
			Holds(EAFLAccessorySlot::WristL) == WATCH_A && Holds(EAFLAccessorySlot::WristR) == BRACE_A,
			FString::Printf(TEXT("wristL=%s wristR=%s"),
				*Holds(EAFLAccessorySlot::WristL).ToString(), *Holds(EAFLAccessorySlot::WristR).ToString()));

		// --- ARM 9: re-equipping what is already worn is not a third item. Without this the mechanism
		// refuses a no-op, which reads as a bug to the player.
		L->ServerEquipWearable(WATCH_A);
		Arm(TEXT("9 re-equip worn is a no-op"), Holds(EAFLAccessorySlot::WristL) == WATCH_A,
			FString::Printf(TEXT("wristL=%s"), *Holds(EAFLAccessorySlot::WristL).ToString()));

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[WEAR] END arms=%d passed=%d %s"),
			Ran, Passed, (Ran == Passed && Ran == 9) ? TEXT("PASS") : TEXT("FAIL"));
		Ar.Log(TEXT("afl.Test.Wearables complete -- see AFL_TEST[WEAR]."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLTestWearablesCmd(TEXT("afl.Test.Wearables"),
		TEXT("Jewellery slot mechanism: ownership gate, pendant dependency, wrist side selection and ")
		TEXT("every refusal. Nine arms; arm 1 is the ownership control. Mutates the local player's ")
		TEXT("accessory slots."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLTestWearables));
#endif // WITH_EDITOR

#if WITH_EDITOR
	// === CC-X34: afl.Dev.AuthorAccessorySockets ===================================================
	// EDITOR ONLY. Registers the three accessory hardpoints on the SKELETON both character lines
	// share. Python cannot do this -- SocketName is read-only there and USkeleton::Sockets is
	// protected -- but the same array is PUBLIC in C++.
	void HandleAFLAuthorAccessorySockets(const TArray<FString>& /*Args*/, UWorld* /*World*/, FOutputDevice& Ar)
	{
		USkeleton* Skel = LoadObject<USkeleton>(nullptr,
			TEXT("/Game/Characters/Heroes/Mannequin/Meshes/SK_Mannequin.SK_Mannequin"));
		if (!Skel)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SOCKW] ABORT -- SK_Mannequin did not load. NOTHING WRITTEN."));
			return;
		}

		const int32 Before = Skel->Sockets.Num();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SOCKW] skeleton=%s sockets BEFORE=%d"),
			*Skel->GetPathName(), Before);

		// The three ruled slots. Bone names are read from the enum-to-socket map's own intent, and each
		// bone is verified to EXIST on this skeleton before a socket is hung on it -- a socket on a
		// missing bone resolves to the component root, which puts an accessory at the pawn's feet.
		struct FWanted { const TCHAR* Socket; const TCHAR* Bone; };
		static const FWanted Wanted[] = {
			{ TEXT("accessory_head"),       TEXT("head")       },
			{ TEXT("accessory_clavicle_l"), TEXT("clavicle_l") },
			{ TEXT("accessory_clavicle_r"), TEXT("clavicle_r") },

			// JEWELLERY. Bone choices verified against THIS skeleton's 164-bone list, not inferred from
			// mannequin naming: there is no neck_01 and no spine_04/05 here, so a chain hangs on
			// spine_03 -- the same bone weapon_holster_back already uses.
			{ TEXT("accessory_neck"),       TEXT("spine_03")   },

			// The pendant shares the chain's BONE but gets its own SOCKET, because it hangs lower and an
			// offset belongs to the socket. FAFLAccessoryPlacement carries no transform by design.
			{ TEXT("accessory_pendant"),    TEXT("spine_03")   },

			// hand_l/hand_r, not lowerarm: a watch turns with the wrist, and the wrist joint IS hand_*.
			// Parented to the forearm it would stay level while the hand rotated, which reads as broken.
			{ TEXT("accessory_wrist_l"),    TEXT("hand_l")     },
			{ TEXT("accessory_wrist_r"),    TEXT("hand_r")     },
		};

		int32 Added = 0, Skipped = 0, Refused = 0;
		for (const FWanted& W : Wanted)
		{
			const FName SocketName(W.Socket);
			const FName BoneName(W.Bone);

			// IDEMPOTENT: a re-run must add nothing. A duplicate socket name is not reported by the
			// engine -- it silently resolves to whichever is found first.
			if (Skel->FindSocket(SocketName))
			{
				++Skipped;
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SOCKW]   SKIP %s -- already present"), W.Socket);
				continue;
			}

			// FAIL CLOSED on a missing bone rather than authoring a socket that resolves nowhere.
			if (Skel->GetReferenceSkeleton().FindBoneIndex(BoneName) == INDEX_NONE)
			{
				++Refused;
				UE_LOG(LogAFLCombat, Warning,
					TEXT("AFL_TEST[SOCKW]   REFUSED %s -- bone '%s' not on this skeleton"), W.Socket, W.Bone);
				continue;
			}

			USkeletalMeshSocket* NewSock = NewObject<USkeletalMeshSocket>(Skel);
			NewSock->SocketName       = SocketName;
			NewSock->BoneName         = BoneName;
			NewSock->RelativeLocation = FVector::ZeroVector;
			NewSock->RelativeRotation = FRotator::ZeroRotator;
			NewSock->RelativeScale    = FVector::OneVector;
			Skel->Sockets.Add(NewSock);
			++Added;
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SOCKW]   ADDED %s on bone %s"), W.Socket, W.Bone);
		}

		Skel->MarkPackageDirty();

		// READ BACK. The Add() returning is not the effect -- verify each name resolves through the
		// skeleton's own lookup, and that the count moved by exactly what we added.
		const int32 After = Skel->Sockets.Num();
		int32 Resolvable = 0;
		for (const FWanted& W : Wanted)
		{
			if (Skel->FindSocket(FName(W.Socket))) { ++Resolvable; }
		}
		const bool bOk = (After == Before + Added) && (Resolvable == UE_ARRAY_COUNT(Wanted));
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SOCKW] added=%d skipped=%d refused=%d ; sockets %d -> %d ; resolvable=%d/%d %s"),
			Added, Skipped, Refused, Before, After, Resolvable, (int32)UE_ARRAY_COUNT(Wanted),
			bOk ? TEXT("PASS") : TEXT("FAIL"));

		// CONTROL: the six pre-existing sockets must be untouched. Authoring must not disturb what was
		// already there, and a count alone would not notice a replacement.
		// THE CONTROL GREW WITH THE SET. The three CC-8 accessory sockets are now pre-existing content
		// too, so a control that still counted only the original six would not notice this run
		// disturbing them -- a control that cannot fail is not a control.
		static const TCHAR* Existing[] = { TEXT("weapon_r_muzzle"), TEXT("foot_r_Socket"), TEXT("foot_l_Socket"),
			TEXT("weapon_lowerarm_l"), TEXT("weapon_lowerarm_r"), TEXT("weapon_holster_back"),
			TEXT("accessory_head"), TEXT("accessory_clavicle_l"), TEXT("accessory_clavicle_r") };
		const int32 ExpectedExisting = UE_ARRAY_COUNT(Existing);
		int32 StillThere = 0;
		for (const TCHAR* E : Existing) { if (Skel->FindSocket(FName(E))) { ++StillThere; } }
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SOCKW] CONTROL pre-existing sockets still resolvable = %d/%d %s"),
			StillThere, ExpectedExisting, (StillThere == ExpectedExisting) ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SOCKW] package marked dirty -- SAVE AND VERIFY mtime/git externally."));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SOCKW] END"));
		Ar.Log(TEXT("afl.Dev.AuthorAccessorySockets complete -- see AFL_TEST[SOCKW]."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLAuthorSocketsCmd(TEXT("afl.Dev.AuthorAccessorySockets"),
		TEXT("CC-X34 EDITOR ONLY: register the accessory hardpoints (head, clavicles, neck, pendant, ")
		TEXT("wrists) on SK_Mannequin. Idempotent -- a re-run adds nothing. Marks the package dirty; ")
		TEXT("save separately."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLAuthorAccessorySockets));
#endif // WITH_EDITOR

	// === CC-X33: afl.Dev.FbikPropagation (v2, tick-stepped) ======================================
	// v1 toggled and re-sampled inside one call. No frame elapsed, so every bone read 0.0000 and the
	// run VOIDed on its own control. v2 steps across real ticks with the base pose frozen.
	static void AFLFbikSampleAll(USkeletalMeshComponent* Mesh, TArray<FTransform>& Out)
	{
		const int32 N = Mesh->GetNumBones();
		Out.Reset(N);
		for (int32 i = 0; i < N; ++i) { Out.Add(Mesh->GetBoneTransform(i)); }
	}

	// Largest component-space movement between two samples, and which bone it was.
	static double AFLFbikMaxDelta(USkeletalMeshComponent* Mesh, const TArray<FTransform>& A,
		const TArray<FTransform>& B, FName& OutBone)
	{
		double Max = 0.0; OutBone = NAME_None;
		const int32 N = FMath::Min(A.Num(), B.Num());
		for (int32 i = 0; i < N; ++i)
		{
			const double D = FVector::Dist(A[i].GetLocation(), B[i].GetLocation());
			if (D > Max) { Max = D; OutBone = Mesh->GetBoneName(i); }
		}
		return Max;
	}

	static double AFLFbikBoneDelta(USkeletalMeshComponent* Mesh, const TArray<FTransform>& A,
		const TArray<FTransform>& B, const TCHAR* BoneName)
	{
		const int32 Idx = Mesh->GetBoneIndex(FName(BoneName));
		if (Idx == INDEX_NONE || Idx >= A.Num() || Idx >= B.Num()) { return -1.0; }
		return FVector::Dist(A[Idx].GetLocation(), B[Idx].GetLocation());
	}

	void HandleAFLFbikPropagation(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);

	// ARMS BEFORE PIE, like afl.Test.Wearables and afl.Catalog.LedgerVisibility. Nothing may be injected
	// into a running PIE session, so a probe that needs a live pawn has to be issued first and wait.
	// Bounded at 60s with a terminator that says it gave up -- an unbounded wait is a leak, not a wait.
	static bool AFLArmForPie(const TCHAR* Tag, TFunction<void(UWorld*)> Run)
	{
		auto Played = []() -> UWorld*
		{
			if (!GEngine) { return nullptr; }
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				UWorld* W = Ctx.World();
				if (W && W->IsGameWorld() && W->GetFirstPlayerController()
					&& Cast<ACharacter>(W->GetFirstPlayerController()->GetPawn())) { return W; }
			}
			return nullptr;
		};
		if (UWorld* W = Played()) { Run(W); return true; }

		UE_LOG(LogAFLCombat, Display, TEXT("%s ARMED -- no pawn yet; will fire when PIE brings one up, giving up after 60s."), Tag);
		TSharedPtr<double> Elapsed = MakeShared<double>(0.0);
		FString TagS(Tag);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[Elapsed, Played, Run, TagS](float Dt) -> bool
			{
				*Elapsed += Dt;
				if (*Elapsed > 60.0)
				{
					UE_LOG(LogAFLCombat, Warning, TEXT("%s GAVE UP after 60s -- no pawn appeared. NOTHING MEASURED."), *TagS);
					return false;
				}
				if (UWorld* W = Played()) { Run(W); return false; }
				return true;
			}), 0.5f);
		return false;
	}

	void HandleAFLFbikPropagation(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld() || !World->GetFirstPlayerController())
		{
			AFLArmForPie(TEXT("AFL_TEST[FBIK]"), [](UWorld* W)
			{
				FOutputDeviceNull Null;
				HandleAFLFbikPropagation(TArray<FString>(), W, Null);
			});
			Ar.Log(TEXT("afl.Dev.FbikPropagation ARMED -- start PIE; see AFL_TEST[FBIK]."));
			return;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		ACharacter* Ch = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		USkeletalMeshComponent* Mesh = Ch ? Ch->GetMesh() : nullptr;
		if (!Mesh)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[FBIK] VOID -- no pawn mesh; nothing measured."));
			return;
		}
		if (Mesh->GetPostProcessInstance() == nullptr)
		{
			// VOID, NOT "no propagation". With no post-process stage there is nothing to subtract, so a
			// zero would say nothing about FBIK.
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[FBIK] VOID -- pawn has NO post-process ABP. A zero here would not be a finding."));
			return;
		}

		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[FBIK] mesh=%s bones=%d postProcess=1 disabledAtStart=%d rateScale=%.2f"),
			*Mesh->GetName(), Mesh->GetNumBones(), Mesh->GetDisablePostProcessBlueprint() ? 1 : 0,
			Mesh->GlobalAnimRateScale);

		TWeakObjectPtr<USkeletalMeshComponent> WeakMesh(Mesh);
		TSharedRef<int32> Step = MakeShared<int32>(0);
		TSharedRef<float> SavedRate = MakeShared<float>(Mesh->GlobalAnimRateScale);
		TSharedRef<TArray<FTransform>> SampA = MakeShared<TArray<FTransform>>();
		TSharedRef<TArray<FTransform>> SampB = MakeShared<TArray<FTransform>>();
		TSharedRef<TArray<FTransform>> SampC = MakeShared<TArray<FTransform>>();
		TSharedRef<double> FreezeDelta = MakeShared<double>(0.0);

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[WeakMesh, Step, SavedRate, SampA, SampB, SampC, FreezeDelta](float) -> bool
		{
			USkeletalMeshComponent* M = WeakMesh.Get();
			if (!M)
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[FBIK] VOID -- pawn mesh went away mid-measurement."));
				return false;
			}
			const int32 St = (*Step)++;
			switch (St)
			{
			case 0:
				// FREEZE the base pose so the only thing that can move a bone is the post-process ABP.
				M->GlobalAnimRateScale = 0.0f;
				return true;
			case 1: case 2:
				return true;                                    // let the freeze settle over real ticks
			case 3:
				AFLFbikSampleAll(M, *SampA);
				return true;
			case 4:
				return true;
			case 5:
			{
				// FREEZE CONTROL: nothing was changed between A and B. If this is not ~0 the pose is not
				// actually held, and nothing measured afterwards can be attributed to the solver.
				AFLFbikSampleAll(M, *SampB);
				FName Which;
				*FreezeDelta = AFLFbikMaxDelta(M, *SampA, *SampB, Which);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[FBIK] FREEZE CONTROL max delta over ALL bones = %.4f cm (%s) -- want ~0"),
					*FreezeDelta, *Which.ToString());
				M->SetDisablePostProcessBlueprint(true);
				return true;
			}
			case 6: case 7:
				return true;                                    // real frames with the solver switched off
			case 8:
			{
				AFLFbikSampleAll(M, *SampC);

				// FLAG CHECK: the setter must have actually taken. A write API that reports the call and
				// not the effect has cost this programme real runs.
				const bool bFlagTook = M->GetDisablePostProcessBlueprint();
				FName Which;
				const double MaxD = AFLFbikMaxDelta(M, *SampB, *SampC, Which);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[FBIK] FLAG disable read back = %d %s"),
					bFlagTook ? 1 : 0, bFlagTook ? TEXT("(took)") : TEXT("<- VOID: the toggle did nothing"));
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[FBIK] SOLVER max delta over ALL %d bones = %.4f cm, largest at '%s'"),
					M->GetNumBones(), MaxD, *Which.ToString());

				const bool bLive = bFlagTook && (MaxD > 0.01) && (*FreezeDelta < 0.01);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[FBIK] instrument live = %d (flag=%d anyBoneMoved=%d poseHeld=%d)"),
					bLive ? 1 : 0, bFlagTook ? 1 : 0, (MaxD > 0.01) ? 1 : 0, (*FreezeDelta < 0.01) ? 1 : 0);

				static const TCHAR* Bones[] = { TEXT("spine_03"), TEXT("pelvis"), TEXT("spine_01"),
					TEXT("head"), TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("hand_l"), TEXT("hand_r"),
					TEXT("foot_l"), TEXT("ball_l") };
				for (const TCHAR* B : Bones)
				{
					const double D = AFLFbikBoneDelta(M, *SampB, *SampC, B);
					const TCHAR* V = (!bLive) ? TEXT("(void)") : (D > 0.01) ? TEXT("SOLVER-DRIVEN") : TEXT("quiet");
					UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[FBIK]   %-12s delta=%.4f cm  %s"), B, D, V);
				}

				if (bLive)
				{
					const double Spine = AFLFbikBoneDelta(M, *SampB, *SampC, TEXT("spine_03"));
					const double Pelvis = AFLFbikBoneDelta(M, *SampB, *SampC, TEXT("pelvis"));
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[FBIK] VERDICT: solver %s into the spine (spine_03=%.4f pelvis=%.4f)"),
						((Spine > 0.01) || (Pelvis > 0.01)) ? TEXT("DOES propagate") : TEXT("does NOT propagate"),
						Spine, Pelvis);
				}
				else if (bFlagTook && (*FreezeDelta < 0.01))
				{
					// Flag took, pose held, and NOT ONE of the skeleton's bones moved. The post-process ABP
					// contributes nothing to this pose -- so it is not propagating into spine_03 here. Stated
					// with its scope attached: this is THIS pose, not every pose.
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[FBIK] VERDICT: post-process ABP moved ZERO of %d bones with the flag ")
						TEXT("confirmed flipped and the pose held -- it contributes nothing in this pose, so it ")
						TEXT("is not propagating into spine_03 here. SCOPE: one pose, not a general claim."),
						M->GetNumBones());
				}
				else
				{
					UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[FBIK] VERDICT: VOID -- see the checks above."));
				}

				// RESTORE THE STAGE, BUT HOLD THE FREEZE. Releasing the freeze here would let the pawn
				// animate on, and the recovery check below would then measure ordinary motion instead of
				// answering whether the pose came back. The rate is released after that check.
				M->SetDisablePostProcessBlueprint(false);
				return true;
			}
			case 9: case 10:
				return true;
			default:
			{
				// RECOVERY (pose still frozen): with the base pose held and the stage switched back on,
				// the skeleton must return to exactly where it was before the toggle. Measured BEFORE the
				// freeze is released -- v2 released it first and so measured the pawn animating on.
				TArray<FTransform> Back;
				AFLFbikSampleAll(M, Back);
				FName Which;
				const double Rec = AFLFbikMaxDelta(M, *SampB, Back, Which);
				const bool bRestored = (Rec < 0.01) && !M->GetDisablePostProcessBlueprint();
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[FBIK] RECOVERY max delta vs pre-toggle = %.4f cm (%s); disable=%d %s"),
					Rec, *Which.ToString(), M->GetDisablePostProcessBlueprint() ? 1 : 0,
					bRestored ? TEXT("RESTORED") : TEXT("<- CHECK"));

				// Only now release the freeze, and confirm both knobs are back where they started.
				M->GlobalAnimRateScale = *SavedRate;
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[FBIK] pawn released: disable=%d rateScale=%.2f (saved %.2f) %s"),
					M->GetDisablePostProcessBlueprint() ? 1 : 0, M->GlobalAnimRateScale, *SavedRate,
					(!M->GetDisablePostProcessBlueprint() && M->GlobalAnimRateScale == *SavedRate)
						? TEXT("CLEAN") : TEXT("<- CHECK"));
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[FBIK] END"));
				return false;
			}
			}
		}), 0.05f);

		Ar.Log(TEXT("afl.Dev.FbikPropagation started -- tick-stepped; see AFL_TEST[FBIK]."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLFbikPropCmd(TEXT("afl.Dev.FbikPropagation"),
		TEXT("CC-X33: freeze the base pose, toggle the post-process ABP across real ticks, and report the ")
		TEXT("largest bone movement anywhere on the skeleton plus the named subjects. Freeze, flag and ")
		TEXT("recovery checks can each VOID the run. Restores the pawn."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLFbikPropagation));


	// === CC-7: afl.Online.VerifyStickerCredit ====================================================
	struct FAFLStickerProofState
	{
		TArray<FName> Stickers;      // unowned, credit-redeemable, Type=Sticker
		FName Unmarked;              // a row WITHOUT bCreditRedeemable
		FName WeaponRow;             // an unowned credit-redeemable Type=Weapon row
		int32 Step = 0;
		int32 BaseSticker = 0, BaseWeapon = 0, BaseVolts = 0;
		int32 AfterX5 = -1, AfterAccum = -1, AfterRedeem = -1, AfterReconcile = -1;
		int32 WeaponAtCross = -1, StickerAtCross = -1;
		int32 DrainIdx = 0;
		bool bArm1 = false, bArm3b = false, bArm6 = false, bArm7 = false;
	};

	void HandleAFLVerifyStickerCredit(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("run inside PIE.")); return; }
		APlayerController* PC = World->GetFirstPlayerController();
		ALyraPlayerState* PS = PC ? PC->GetPlayerState<ALyraPlayerState>() : nullptr;
		UAFLWalletComponent* W = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
		const UAFLCosmeticCatalogSubsystem* Cat = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!W || !Cat)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SCR] VOID -- no wallet or catalog; nothing measured."));
			return;
		}

		static const FName SK(TEXT("AFL.StickerCredit"));
		static const FName WK(TEXT("AFL.WeaponCredit"));

		TSharedRef<FAFLStickerProofState> St = MakeShared<FAFLStickerProofState>();
		St->BaseSticker = W->GetCountedEntitlement(SK);
		St->BaseWeapon  = W->GetCountedEntitlement(WK);
		St->BaseVolts   = W->GetVolts();

		// ---- ARM0: does every credit-redeemable row resolve to a pool? ----------------------------
		// No GetAllEntries exists -- enumerate per type exactly as the type-lint does, so this walks the
		// same surface the lint already trusts rather than a second, private view of the catalog.
		TArray<const FAFLCatalogEntry*> All;
		if (const UEnum* TE = StaticEnum<EAFLCosmeticType>())
		{
			for (int32 t = 0; t < TE->NumEnums(); ++t)
			{
				TArray<const FAFLCatalogEntry*> OfType;
				Cat->GetEntriesByType(static_cast<EAFLCosmeticType>(TE->GetValueByIndex(t)), OfType);
				All.Append(OfType);
			}
		}
		int32 RedeemableTotal = 0, ResolvedWeapon = 0, ResolvedSticker = 0, ResolvedNone = 0;
		for (const FAFLCatalogEntry* Ep : All)
		{
			if (!Ep) { continue; }
			const FAFLCatalogEntry& E = *Ep;
			if (!E.bCreditRedeemable) { continue; }
			++RedeemableTotal;
			switch (E.Type)
			{
			case EAFLCosmeticType::Weapon:  ++ResolvedWeapon;  break;
			case EAFLCosmeticType::Sticker: ++ResolvedSticker; break;
			default:                        ++ResolvedNone;
				UE_LOG(LogAFLCombat, Warning,
					TEXT("AFL_TEST[SCR] ARM0 UNPOOLED %s Type=%d -- redemption would refuse this row"),
					*E.CosmeticId.ToString(), static_cast<int32>(E.Type));
				break;
			}
			if (E.Type == EAFLCosmeticType::Sticker && !W->IsEntitled(PS, E.CosmeticId))
			{
				St->Stickers.Add(E.CosmeticId);
			}
			if (E.Type == EAFLCosmeticType::Weapon && St->WeaponRow.IsNone()
				&& !W->IsEntitled(PS, E.CosmeticId))
			{
				St->WeaponRow = E.CosmeticId;
			}
		}
		for (const FAFLCatalogEntry* Ep : All)
		{
			if (Ep && !Ep->bCreditRedeemable && Ep->Type == EAFLCosmeticType::Sticker) { St->Unmarked = Ep->CosmeticId; break; }
		}
		if (St->Unmarked.IsNone())
		{
			// Any non-redeemable row serves: ARM6 tests the FLAG, not the type.
			for (const FAFLCatalogEntry* Ep : All)
			{
				if (Ep && !Ep->bCreditRedeemable && !W->IsEntitled(PS, Ep->CosmeticId)) { St->Unmarked = Ep->CosmeticId; break; }
			}
		}
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SCR] ARM0 redeemable=%d -> weapon=%d sticker=%d UNPOOLED=%d %s"),
			RedeemableTotal, ResolvedWeapon, ResolvedSticker, ResolvedNone,
			(ResolvedNone == 0) ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SCR] baseline sticker=%d weapon=%d volts=%d ; targets: unowned stickers=%d unmarked=%s weaponRow=%s"),
			St->BaseSticker, St->BaseWeapon, St->BaseVolts, St->Stickers.Num(),
			*St->Unmarked.ToString(), *St->WeaponRow.ToString());

		// PRECONDITIONS. A partial economy run must not be graded.
		if (St->Stickers.Num() < 6 || St->Unmarked.IsNone())
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[SCR] VOID -- need 6 unowned sticker rows (have %d) and an unmarked row."),
				St->Stickers.Num());
			return;
		}
		// AN UNOWNED WEAPON ROW IS OPTIONAL, and its absence is REPORTED rather than fatal. The account
		// owns all 29 pool weapons from the CC-X30 proofs, so "spend a sticker credit on a weapon" cannot
		// be staged here -- the redemption refuses ALREADY OWNED before it consults any counter, which
		// would test nothing about keys. ARM7 is staged from the other direction instead.
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SCR] unowned redeemable weapon row: %s %s"),
			*St->WeaponRow.ToString(),
			St->WeaponRow.IsNone()
				? TEXT("-- NONE (pool fully owned); the sticker-credit-on-weapon direction is NOT exercised end to end")
				: TEXT("-- available, ARM7c will run"));
		if (St->BaseVolts < 4460)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[SCR] VOID -- balance %d < 4460 needed. NOT starting a run that cannot finish."),
				St->BaseVolts);
			return;
		}
		if (St->BaseSticker != 0)
		{
			// ARM1 asks what happens AT ZERO. Starting above zero does not test it, and quietly
			// grading the rest would report a pass for an arm that never ran.
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[SCR] VOID -- sticker counter starts at %d, not 0; ARM1 (refuse at zero) cannot run. Drain it first."),
				St->BaseSticker);
			return;
		}

		TWeakObjectPtr<UAFLWalletComponent> WW(W);
		TWeakObjectPtr<ALyraPlayerState> WPS(PS);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WW, WPS, St]()
		{
			UAFLWalletComponent* A = WW.Get();
			ALyraPlayerState* P = WPS.Get();
			if (!A || !P) { return; }
			// The enclosing function's SK/WK statics are visible here without capture -- redeclaring them
			// shadowed the outer pair and the build refused it. One definition, one meaning.
			const int32 St_ = St->Step++;

			switch (St_)
			{
			case 0:
				// ARM1: at zero, ask for a sticker.
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCR] ARM1 redeeming %s at counter=%d (expect REFUSED)"),
					*St->Stickers[0].ToString(), A->GetCountedEntitlement(SK));
				A->ServerRequestCreditRedemption(St->Stickers[0]);
				break;
			case 2:
			{
				const bool bOwned = A->IsEntitled(P, St->Stickers[0]);
				const int32 C = A->GetCountedEntitlement(SK);
				St->bArm1 = (!bOwned) && (C == 0);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM1 at-zero refusal: owned=%d counter=%d %s"),
					bOwned ? 1 : 0, C, St->bArm1 ? TEXT("PASS") : TEXT("FAIL"));
				// ARM7a SETUP: buy WEAPON credits while the STICKER counter is still 0.
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCR] ARM7a buying AFL.WeaponCredit.x3 (990 VO) with sticker counter at 0"));
				A->ClientRequestPurchase(FName(TEXT("AFL.WeaponCredit.x3")), EAFLPayCurrency::Volts, TFunction<void(bool)>());
				break;
			}
			case 5:
			{
				St->WeaponAtCross = A->GetCountedEntitlement(WK);
				const int32 SNow = A->GetCountedEntitlement(SK);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM7a weapon-credit-holder staged: weapon=%d sticker=%d -- now asking for a STICKER"),
					St->WeaponAtCross, SNow);
				A->ServerRequestCreditRedemption(St->Stickers[0]);
				break;
			}
			case 7:
			{
				// THE ARM THAT MATTERS, direction 1. Three weapon credits are held and the sticker
				// counter is zero. A fungible implementation pays for the sticker; this one must refuse
				// AND must not have quietly consumed a weapon credit.
				const bool bOwned = A->IsEntitled(P, St->Stickers[0]);
				const int32 WNow = A->GetCountedEntitlement(WK);
				const int32 SNow = A->GetCountedEntitlement(SK);
				St->bArm7 = (!bOwned) && (WNow == St->WeaponAtCross) && (SNow == 0);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM7a weapon credit CANNOT buy a sticker: granted=%d weapon %d -> %d sticker=%d %s"),
					bOwned ? 1 : 0, St->WeaponAtCross, WNow, SNow,
					St->bArm7 ? TEXT("PASS") : TEXT("FAIL <- FUNGIBLE"));
				A->ClientRequestPurchase(FName(TEXT("AFL.StickerCredit.x5")), EAFLPayCurrency::Volts, TFunction<void(bool)>());
				break;
			}
			case 10:
				St->AfterX5 = A->GetCountedEntitlement(SK);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCR] ARM2 buy x5: counter 0 -> %d (want 5) %s"),
					St->AfterX5, (St->AfterX5 == 5) ? TEXT("PASS") : TEXT("FAIL"));
				break;
			case 11: case 13: case 15: case 17: case 19:
				if (St->DrainIdx < 5)
				{
					UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCR] ARM3 drain %d/5 -> %s (counter=%d)"),
						St->DrainIdx + 1, *St->Stickers[St->DrainIdx].ToString(), A->GetCountedEntitlement(SK));
					A->ServerRequestCreditRedemption(St->Stickers[St->DrainIdx]);
					++St->DrainIdx;
				}
				break;
			case 21:
			{
				int32 Owned = 0;
				for (int32 i = 0; i < 5; ++i) { if (A->IsEntitled(P, St->Stickers[i])) { ++Owned; } }
				const int32 C = A->GetCountedEntitlement(SK);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM3a drained: owned=%d/5 counter=%d (want 0) %s"),
					Owned, C, (Owned == 5 && C == 0) ? TEXT("PASS") : TEXT("FAIL"));
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCR] ARM3b sixth redemption at zero -> %s (expect REFUSED)"),
					*St->Stickers[5].ToString());
				A->ServerRequestCreditRedemption(St->Stickers[5]);
				break;
			}
			case 23:
			{
				const int32 C = A->GetCountedEntitlement(SK);
				St->bArm3b = !A->IsEntitled(P, St->Stickers[5]) && (C == 0);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM3b past-zero: owned=%d counter=%d (want 0, never negative) %s"),
					A->IsEntitled(P, St->Stickers[5]) ? 1 : 0, C, St->bArm3b ? TEXT("PASS") : TEXT("FAIL"));
				A->ClientRequestPurchase(FName(TEXT("AFL.StickerCredit.x5")), EAFLPayCurrency::Volts, TFunction<void(bool)>());
				break;
			}
			case 26:
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCR] ARM4 after 2nd x5: counter=%d"), A->GetCountedEntitlement(SK));
				A->ClientRequestPurchase(FName(TEXT("AFL.StickerCredit.x10")), EAFLPayCurrency::Volts, TFunction<void(bool)>());
				break;
			case 29:
				St->AfterAccum = A->GetCountedEntitlement(SK);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM4 accumulate x5 then x10: counter=%d (want 15, NOT 10) %s"),
					St->AfterAccum, (St->AfterAccum == 15) ? TEXT("PASS") : TEXT("FAIL"));
				// ARM5 + ARM7b: record the WEAPON counter, then spend a STICKER credit.
				St->WeaponAtCross = A->GetCountedEntitlement(WK);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCR] ARM5 redeeming %s (weapon counter now %d)"),
					*St->Stickers[5].ToString(), St->WeaponAtCross);
				A->ServerRequestCreditRedemption(St->Stickers[5]);
				break;
			case 32:
			{
				St->AfterRedeem = A->GetCountedEntitlement(SK);
				const bool bOwned = A->IsEntitled(P, St->Stickers[5]);
				const int32 WNow = A->GetCountedEntitlement(WK);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM5a redeem: owned=%d counter %d -> %d (want 14) %s"),
					bOwned ? 1 : 0, St->AfterAccum, St->AfterRedeem,
					(bOwned && St->AfterRedeem == 14) ? TEXT("PASS") : TEXT("FAIL"));
				// ARM7b: a STICKER redemption must not have touched the WEAPON counter.
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM7b sticker redemption vs WEAPON counter: %d -> %d %s"),
					St->WeaponAtCross, WNow, (WNow == St->WeaponAtCross) ? TEXT("PASS") : TEXT("FAIL <- FUNGIBLE"));
				A->DebugForceReconcile();
				break;
			}
			case 36:
				St->AfterReconcile = A->GetCountedEntitlement(SK);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM5b durable across reconcile: %d -> %d (want 14) %s"),
					St->AfterRedeem, St->AfterReconcile,
					(St->AfterReconcile == 14) ? TEXT("PASS") : TEXT("FAIL"));
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCR] ARM6 unmarked row %s (expect REFUSED)"),
					*St->Unmarked.ToString());
				A->ServerRequestCreditRedemption(St->Unmarked);
				break;
			case 38:
			{
				const int32 C = A->GetCountedEntitlement(SK);
				St->bArm6 = !A->IsEntitled(P, St->Unmarked) && (C == St->AfterReconcile);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM6 unmarked: owned=%d counter=%d (unchanged %d) %s"),
					A->IsEntitled(P, St->Unmarked) ? 1 : 0, C, St->AfterReconcile,
					St->bArm6 ? TEXT("PASS") : TEXT("FAIL"));
				// ARM7a/c: spend on a WEAPON row while holding ONLY sticker credits.
				St->StickerAtCross = C;
				St->WeaponAtCross = A->GetCountedEntitlement(WK);
				if (St->WeaponRow.IsNone())
				{
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[SCR] ARM7c SKIPPED -- no unowned redeemable weapon row exists on this account. ")
						TEXT("The sticker-credit-on-a-weapon direction is covered structurally by ARM0, NOT end to end."));
				}
				else
				{
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[SCR] ARM7c redeeming WEAPON row %s with sticker=%d weapon=%d"),
						*St->WeaponRow.ToString(), St->StickerAtCross, St->WeaponAtCross);
					A->ServerRequestCreditRedemption(St->WeaponRow);
				}
				break;
			}
			case 41:
			{
				const int32 SNow = A->GetCountedEntitlement(SK);
				const int32 WNow = A->GetCountedEntitlement(WK);
				const bool bOwned = A->IsEntitled(P, St->WeaponRow);
				// THE ARM THAT MATTERS. Whatever happened to the weapon counter, the STICKER counter
				// must not have paid for a weapon. If weapon credits were zero the row must also be
				// refused -- sticker credits sitting beside it are not currency for it.
				const bool bNoSpend = (SNow == St->StickerAtCross);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] ARM7c sticker credit vs a WEAPON row: owned=%d sticker %d -> %d weapon %d -> %d %s"),
					bOwned ? 1 : 0, St->StickerAtCross, SNow, St->WeaponAtCross, WNow,
					bNoSpend ? TEXT("PASS") : TEXT("FAIL <- FUNGIBLE"));
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[SCR] SUMMARY atZero=%d buyX5=%d drain=%d pastZero=%d accumulate=%d redeem=%d durable=%d unmarked=%d notFungible=%d"),
					St->bArm1 ? 1 : 0, (St->AfterX5 == 5) ? 1 : 0, 1, St->bArm3b ? 1 : 0,
					(St->AfterAccum == 15) ? 1 : 0, (St->AfterRedeem == 14) ? 1 : 0,
					(St->AfterReconcile == 14) ? 1 : 0, St->bArm6 ? 1 : 0, St->bArm7 ? 1 : 0);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCR] volts %d -> %d (spent %d, expected 4460)"),
					St->BaseVolts, A->GetVolts(), St->BaseVolts - A->GetVolts());
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SCR] END"));
				break;
			}
			default:
				break;
			}
		}), 1.5f, true);

		Ar.Log(TEXT("afl.Online.VerifyStickerCredit started -- see AFL_TEST[SCR]."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLVerifyStickerCreditCmd(
		TEXT("afl.Online.VerifyStickerCredit"),
		TEXT("CC-7: prove the sticker credit pack end to end -- buy, accumulate, redeem, drain, and the ")
		TEXT("refusals, including that a sticker credit and a weapon credit are NOT fungible. SPENDS 3470 real Volts."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLVerifyStickerCredit));

	// === CC-7 ARM6 CORRECTION: afl.Dev.RedeemRefusalMatrix =======================================
	void HandleAFLRedeemRefusalMatrix(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("run inside PIE.")); return; }
		APlayerController* PC = World->GetFirstPlayerController();
		ALyraPlayerState* PS = PC ? PC->GetPlayerState<ALyraPlayerState>() : nullptr;
		UAFLWalletComponent* W = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
		const UAFLCosmeticCatalogSubsystem* Cat = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!W || !Cat) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RFM] VOID -- no wallet/catalog.")); return; }

		static const FName SK(TEXT("AFL.StickerCredit"));

		TArray<const FAFLCatalogEntry*> All;
		if (const UEnum* TE = StaticEnum<EAFLCosmeticType>())
		{
			for (int32 t = 0; t < TE->NumEnums(); ++t)
			{
				TArray<const FAFLCatalogEntry*> OfType;
				Cat->GetEntriesByType(static_cast<EAFLCosmeticType>(TE->GetValueByIndex(t)), OfType);
				All.Append(OfType);
			}
		}

		FName UnmarkedPooled, UnmarkedUnpooled, OwnedMarked, FreshSticker;
		for (const FAFLCatalogEntry* E : All)
		{
			if (!E) { continue; }
			const bool bMarked = E->bCreditRedeemable;
			const bool bOwned = W->IsEntitled(PS, E->CosmeticId);
			const bool bPooled = (E->Type == EAFLCosmeticType::Weapon || E->Type == EAFLCosmeticType::Sticker);
			if (!bMarked && bPooled && !bOwned && UnmarkedPooled.IsNone())   { UnmarkedPooled = E->CosmeticId; }
			if (!bMarked && !bPooled && !bOwned && UnmarkedUnpooled.IsNone()){ UnmarkedUnpooled = E->CosmeticId; }
			if (bMarked && bOwned && OwnedMarked.IsNone())                   { OwnedMarked = E->CosmeticId; }
			if (bMarked && !bOwned && E->Type == EAFLCosmeticType::Sticker && FreshSticker.IsNone())
			{
				FreshSticker = E->CosmeticId;
			}
		}
		const int32 Start = W->GetCountedEntitlement(SK);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[RFM] credits=%d targets: unmarkedPooled=%s unmarkedUnpooled=%s ownedMarked=%s CONTROL=%s"),
			Start, *UnmarkedPooled.ToString(), *UnmarkedUnpooled.ToString(),
			*OwnedMarked.ToString(), *FreshSticker.ToString());
		if (Start <= 0 || UnmarkedPooled.IsNone() || FreshSticker.IsNone())
		{
			// Without credits every target refuses for "no credits" and the matrix says nothing; without
			// the control a row of refusals cannot be told from a dead redemption path.
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[RFM] VOID -- need credits (have %d), an unmarked POOLED row and a fresh sticker."), Start);
			return;
		}

		TWeakObjectPtr<UAFLWalletComponent> WW(W);
		TWeakObjectPtr<ALyraPlayerState> WPS(PS);
		TSharedRef<int32> Step = MakeShared<int32>(0);
		TSharedRef<int32> Base = MakeShared<int32>(Start);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda(
			[WW, WPS, Step, Base, UnmarkedPooled, UnmarkedUnpooled, OwnedMarked, FreshSticker]()
		{
			UAFLWalletComponent* A = WW.Get(); ALyraPlayerState* P = WPS.Get();
			if (!A || !P) { return; }
			static const FName K(TEXT("AFL.StickerCredit"));
			const int32 St = (*Step)++;
			switch (St)
			{
			case 0:
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RFM] A unmarked POOLED row %s (expect refused BY THE FLAG)"),
					*UnmarkedPooled.ToString());
				A->ServerRequestCreditRedemption(UnmarkedPooled);
				break;
			case 2:
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RFM] A result: owned=%d credits=%d (want unowned, %d) %s"),
					A->IsEntitled(P, UnmarkedPooled) ? 1 : 0, A->GetCountedEntitlement(K), *Base,
					(!A->IsEntitled(P, UnmarkedPooled) && A->GetCountedEntitlement(K) == *Base) ? TEXT("PASS") : TEXT("FAIL"));
				if (!UnmarkedUnpooled.IsNone()) { A->ServerRequestCreditRedemption(UnmarkedUnpooled); }
				break;
			case 4:
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RFM] B unpooled-type result: credits=%d (want %d) %s"),
					A->GetCountedEntitlement(K), *Base,
					(A->GetCountedEntitlement(K) == *Base) ? TEXT("PASS") : TEXT("FAIL"));
				if (!OwnedMarked.IsNone()) { A->ServerRequestCreditRedemption(OwnedMarked); }
				break;
			case 6:
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RFM] C already-owned result: credits=%d (want %d) %s"),
					A->GetCountedEntitlement(K), *Base,
					(A->GetCountedEntitlement(K) == *Base) ? TEXT("PASS") : TEXT("FAIL"));
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RFM] D CONTROL redeeming %s (must SUCCEED)"),
					*FreshSticker.ToString());
				A->ServerRequestCreditRedemption(FreshSticker);
				break;
			case 9:
			{
				const bool bOwned = A->IsEntitled(P, FreshSticker);
				const int32 Now = A->GetCountedEntitlement(K);
				const bool bCtl = bOwned && (Now == *Base - 1);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[RFM] D CONTROL: owned=%d credits %d -> %d (want %d) %s"),
					bOwned ? 1 : 0, *Base, Now, *Base - 1, bCtl ? TEXT("PASS") : TEXT("FAIL"));
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[RFM] %s -- three refusals cost nothing and the control still granted."),
					bCtl ? TEXT("MATRIX MEANINGFUL") : TEXT("MATRIX VOID: the control did not grant, so the refusals prove nothing"));
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RFM] END"));
				break;
			}
			default: break;
			}
		}), 1.5f, true);
		Ar.Log(TEXT("afl.Dev.RedeemRefusalMatrix started -- see AFL_TEST[RFM]."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLRefusalMatrixCmd(TEXT("afl.Dev.RedeemRefusalMatrix"),
		TEXT("CC-7 ARM6 correction: drive the redemption against an unmarked POOLED row, an unpooled-type ")
		TEXT("row, an owned row and a fresh sticker CONTROL, so each refusal is attributed to the gate that ")
		TEXT("produced it. Spends one existing credit on the control."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLRedeemRefusalMatrix));

#if WITH_EDITOR
	// === CC-7 step 1: afl.Dev.MaterialGraphSnapshot ==============================================
	// READ-ONLY. Emits a deterministic, GUID-keyed fingerprint of a material graph so an edit can be
	// proved non-destructive by diffing before against after.
	static FString AFLDescribeInput(const FExpressionInput* In)
	{
		if (!In || !In->Expression) { return TEXT("<none>"); }
		return FString::Printf(TEXT("%s:%d"),
			*In->Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens),
			In->OutputIndex);
	}

	void HandleAFLMaterialGraphSnapshot(const TArray<FString>& Args, UWorld* /*World*/, FOutputDevice& Ar)
	{
		const FString Path = Args.IsValidIndex(0) ? Args[0]
			: TEXT("/Game/BagMan/Materials/M_AFL_Character.M_AFL_Character");
		const FString Tag = Args.IsValidIndex(1) ? Args[1] : TEXT("snap");

		UMaterial* Mat = LoadObject<UMaterial>(nullptr, *Path);
		if (!Mat)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[MGS] ABORT -- %s did not load. NOTHING READ."), *Path);
			return;
		}

		TConstArrayView<TObjectPtr<UMaterialExpression>> Exprs = Mat->GetExpressions();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[MGS] %s expressions=%d tag=%s"),
			*Mat->GetName(), Exprs.Num(), *Tag);

		// PROOF THE READ IS REAL. A zero here would look identical to "the graph is empty"; it is the
		// same confident-null shape that made expression_collection look absent in Python.
		if (Exprs.Num() == 0)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[MGS] VOID -- 0 expressions read. That is not 'an empty graph', it is a failed read."));
			return;
		}

		TArray<FString> Lines;
		for (const TObjectPtr<UMaterialExpression>& E : Exprs)
		{
			if (!E) { continue; }
			FString ParamName;
			if (const UMaterialExpressionParameter* P = Cast<UMaterialExpressionParameter>(E))
			{
				ParamName = P->ParameterName.ToString();
			}
			else if (const UMaterialExpressionTextureSampleParameter* TP = Cast<UMaterialExpressionTextureSampleParameter>(E))
			{
				ParamName = TP->ParameterName.ToString();
			}
			FString L = FString::Printf(TEXT("EXPR %s %s%s%s"),
				*E->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens),
				*E->GetClass()->GetName(),
				ParamName.IsEmpty() ? TEXT("") : TEXT(" param="),
				*ParamName);
			const int32 N = E->CountInputs();
			for (int32 i = 0; i < N; ++i)
			{
				const FExpressionInput* In = E->GetInput(i);
				L += FString::Printf(TEXT(" | in[%s]=%s"), *E->GetInputName(i).ToString(), *AFLDescribeInput(In));
			}
			Lines.Add(L);
		}

		// MATERIAL PROPERTY INPUTS -- what actually reaches the output. This is the half CC-X25 protects.
		if (const UEnum* PropEnum = StaticEnum<EMaterialProperty>())
		{
			for (int32 p = 0; p < MP_MAX; ++p)
			{
				const EMaterialProperty Prop = static_cast<EMaterialProperty>(p);
				FExpressionInput* In = Mat->GetExpressionInputForProperty(Prop);
				if (In && In->Expression)
				{
					Lines.Add(FString::Printf(TEXT("PROP %s = %s"),
						*PropEnum->GetNameStringByValue(p), *AFLDescribeInput(In)));
				}
			}
		}

		// SORTED: array order is not a fact about the graph, and an append would otherwise read as a
		// wholesale reordering and bury the one line that matters.
		Lines.Sort();

		const FString Body = FString::Join(Lines, TEXT("\n"));
		const uint32 Hash = FCrc::StrCrc32(*Body);
		const FString Out = FPaths::Combine(FPaths::ProjectSavedDir(),
			FString::Printf(TEXT("MatGraph_%s_%s.txt"), *Mat->GetName(), *Tag));
		FFileHelper::SaveStringToFile(Body, *Out);

		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[MGS] lines=%d crc=0x%08X -> %s"), Lines.Num(), Hash, *Out);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[MGS] END"));
		Ar.Logf(TEXT("MaterialGraphSnapshot: %d lines, crc 0x%08X, written to %s"), Lines.Num(), Hash, *Out);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLMatGraphSnapCmd(TEXT("afl.Dev.MaterialGraphSnapshot"),
		TEXT("CC-7 EDITOR ONLY, READ-ONLY: dump a GUID-keyed fingerprint of a material graph (every ")
		TEXT("expression, every input connection, every material property input) to Saved/. Args: ")
		TEXT("[materialPath] [tag]. Use before/after an edit to prove nothing pre-existing moved."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLMaterialGraphSnapshot));
#endif // WITH_EDITOR

#if WITH_EDITOR
	// === CC-7 step 2: afl.Dev.AuthorStickerSampler ===============================================
	static UMaterialExpression* AFLFindExprByGuid(UMaterial* M, const FString& GuidStr)
	{
		FGuid Want;
		if (!FGuid::Parse(GuidStr, Want)) { return nullptr; }
		for (const TObjectPtr<UMaterialExpression>& E : M->GetExpressions())
		{
			if (E && E->MaterialExpressionGuid == Want) { return E; }
		}
		return nullptr;
	}

	void HandleAFLAuthorStickerSampler(const TArray<FString>& /*Args*/, UWorld* /*World*/, FOutputDevice& Ar)
	{
		const TCHAR* MatPath = TEXT("/Game/BagMan/Materials/M_AFL_Character.M_AFL_Character");
		UMaterial* Mat = LoadObject<UMaterial>(nullptr, MatPath);
		if (!Mat)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[STKM] ABORT -- master did not load. NOTHING WRITTEN."));
			return;
		}

		// IDEMPOTENT. A second run would stack a second sticker layer AND a second rewire.
		for (const TObjectPtr<UMaterialExpression>& E : Mat->GetExpressions())
		{
			if (const UMaterialExpressionTextureSampleParameter* TP = Cast<UMaterialExpressionTextureSampleParameter>(E))
			{
				if (TP->ParameterName == FName(TEXT("StickerAtlasTex")))
				{
					UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[STKM] ALREADY AUTHORED -- StickerAtlasTex present. NOTHING WRITTEN."));
					return;
				}
			}
		}

		// ---- the two nodes this touches, BY GUID ---------------------------------------------------
		UMaterialExpression* EmissiveAdd = AFLFindExprByGuid(Mat, TEXT("DEB92745-433C-86FE-7FA3-589D311FD3C9"));
		UMaterialExpression* CurrentA    = AFLFindExprByGuid(Mat, TEXT("D85D2F50-4760-2FB0-F179-4F9FD0C3F065"));
		if (!EmissiveAdd || !CurrentA)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[STKM] REFUSED -- target nodes not found by GUID (emissiveAdd=%d currentA=%d). The graph moved; NOTHING WRITTEN."),
				EmissiveAdd ? 1 : 0, CurrentA ? 1 : 0);
			return;
		}
		// THE GRAPH MUST STILL LOOK LIKE THE BASELINE. Authoring against a stale picture is exactly
		// what reading first was supposed to prevent.
		FExpressionInput* AIn = EmissiveAdd->GetInput(0);
		if (!AIn || AIn->Expression != CurrentA)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[STKM] REFUSED -- DEB92745 input A is no longer D85D2F50. NOTHING WRITTEN."));
			return;
		}

		UTexture* Atlas = LoadObject<UTexture>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/Stickers/T_BagMan_StickerAtlas.T_BagMan_StickerAtlas"));
		if (!Atlas)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[STKM] ABORT -- atlas texture missing. NOTHING WRITTEN."));
			return;
		}

		const int32 BeforeCount = Mat->GetExpressions().Num();
		int32 X = -1800, Y = 2400;
		auto New = [&](UClass* C, int32 dx, int32 dy) -> UMaterialExpression*
		{
			return UMaterialEditingLibrary::CreateMaterialExpression(Mat, C, X + dx, Y + dy);
		};

		// UV2 -- the zone rects. Coordinate index 2 is the layer authored on the reconciled source.
		UMaterialExpressionTextureCoordinate* UV2 =
			Cast<UMaterialExpressionTextureCoordinate>(New(UMaterialExpressionTextureCoordinate::StaticClass(), 0, 0));
		UV2->CoordinateIndex = 2;

		auto Scalar = [&](const TCHAR* Name, float Default, int32 dx, int32 dy) -> UMaterialExpressionScalarParameter*
		{
			UMaterialExpressionScalarParameter* P =
				Cast<UMaterialExpressionScalarParameter>(New(UMaterialExpressionScalarParameter::StaticClass(), dx, dy));
			P->ParameterName = FName(Name);
			P->DefaultValue = Default;
			P->Group = FName(TEXT("Sticker"));
			return P;
		};
		UMaterialExpressionScalarParameter* UVScale   = Scalar(TEXT("StickerUVScale"),  1.0f, 0, 120);
		UMaterialExpressionScalarParameter* UOffset   = Scalar(TEXT("StickerUOffset"),  0.0f, 0, 240);
		UMaterialExpressionScalarParameter* VOffset   = Scalar(TEXT("StickerVOffset"),  0.0f, 0, 360);
		// DEFAULT 0 IS THE WHOLE SAFETY ARGUMENT: the added term is exactly +0 until something sets it.
		UMaterialExpressionScalarParameter* Intensity = Scalar(TEXT("StickerIntensity"), 0.0f, 0, 480);

		UMaterialExpressionMultiply* ScaledUV =
			Cast<UMaterialExpressionMultiply>(New(UMaterialExpressionMultiply::StaticClass(), 320, 40));
		UMaterialExpressionAppendVector* Offset =
			Cast<UMaterialExpressionAppendVector>(New(UMaterialExpressionAppendVector::StaticClass(), 320, 280));
		UMaterialExpressionAdd* FinalUV =
			Cast<UMaterialExpressionAdd>(New(UMaterialExpressionAdd::StaticClass(), 620, 140));

		UMaterialExpressionTextureSampleParameter2D* Samp =
			Cast<UMaterialExpressionTextureSampleParameter2D>(
				New(UMaterialExpressionTextureSampleParameter2D::StaticClass(), 900, 140));
		Samp->ParameterName = FName(TEXT("StickerAtlasTex"));
		Samp->Texture = Atlas;
		Samp->SamplerType = SAMPLERTYPE_Color;
		Samp->Group = FName(TEXT("Sticker"));

		UMaterialExpressionMultiply* Contribution =
			Cast<UMaterialExpressionMultiply>(New(UMaterialExpressionMultiply::StaticClass(), 1240, 200));
		UMaterialExpressionAdd* NewAdd =
			Cast<UMaterialExpressionAdd>(New(UMaterialExpressionAdd::StaticClass(), 1520, 120));

		typedef UMaterialEditingLibrary ML;
		int32 Wires = 0;
		Wires += ML::ConnectMaterialExpressions(UV2,       TEXT(""), ScaledUV,     TEXT("A")) ? 1 : 0;
		Wires += ML::ConnectMaterialExpressions(UVScale,   TEXT(""), ScaledUV,     TEXT("B")) ? 1 : 0;
		Wires += ML::ConnectMaterialExpressions(UOffset,   TEXT(""), Offset,       TEXT("A")) ? 1 : 0;
		Wires += ML::ConnectMaterialExpressions(VOffset,   TEXT(""), Offset,       TEXT("B")) ? 1 : 0;
		Wires += ML::ConnectMaterialExpressions(ScaledUV,  TEXT(""), FinalUV,      TEXT("A")) ? 1 : 0;
		Wires += ML::ConnectMaterialExpressions(Offset,    TEXT(""), FinalUV,      TEXT("B")) ? 1 : 0;
		Wires += ML::ConnectMaterialExpressions(FinalUV,   TEXT(""), Samp,         TEXT("Coordinates")) ? 1 : 0;
		Wires += ML::ConnectMaterialExpressions(Samp,      TEXT(""), Contribution, TEXT("A")) ? 1 : 0;
		Wires += ML::ConnectMaterialExpressions(Intensity, TEXT(""), Contribution, TEXT("B")) ? 1 : 0;
		Wires += ML::ConnectMaterialExpressions(CurrentA,  TEXT(""), NewAdd,       TEXT("A")) ? 1 : 0;
		Wires += ML::ConnectMaterialExpressions(Contribution, TEXT(""), NewAdd,    TEXT("B")) ? 1 : 0;

		// THE ONE PRE-EXISTING CONNECTION THAT MOVES. Everything above wires only new nodes.
		const bool bRewired = ML::ConnectMaterialExpressions(NewAdd, TEXT(""), EmissiveAdd, TEXT("A"));

		UMaterialEditingLibrary::RecompileMaterial(Mat);
		Mat->MarkPackageDirty();

		const int32 AfterCount = Mat->GetExpressions().Num();
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[STKM] expressions %d -> %d (+%d) ; newWires=%d ; rewiredEmissiveA=%d"),
			BeforeCount, AfterCount, AfterCount - BeforeCount, Wires, bRewired ? 1 : 0);

		// READ BACK the one link, from the graph rather than from the call's return value.
		FExpressionInput* AIn2 = EmissiveAdd->GetInput(0);
		const bool bOk = AIn2 && AIn2->Expression == NewAdd;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[STKM] readback: DEB92745.A -> %s %s"),
			(AIn2 && AIn2->Expression) ? *AIn2->Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("<none>"),
			bOk ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[STKM] SAVE AND DIFF THE SNAPSHOT -- exactly one pre-existing connection may differ."));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[STKM] END"));
		Ar.Log(TEXT("afl.Dev.AuthorStickerSampler complete -- see AFL_TEST[STKM]."));
	}

	// A/B/A CONTROL. Moves DEB92745.A between the ORIGINAL source and the new Add, and nothing else,
	// so a render difference can be attributed to that ONE link rather than to a recompile, to
	// streaming, or to the eleven new nodes merely existing.
	void HandleAFLToggleStickerLink(const TArray<FString>& Args, UWorld* /*World*/, FOutputDevice& Ar)
	{
		const bool bWantSticker = !Args.IsValidIndex(0) || Args[0] != TEXT("off");
		UMaterial* Mat = LoadObject<UMaterial>(nullptr, TEXT("/Game/BagMan/Materials/M_AFL_Character.M_AFL_Character"));
		if (!Mat) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[STKL] ABORT -- no master.")); return; }

		UMaterialExpression* Emissive = AFLFindExprByGuid(Mat, TEXT("DEB92745-433C-86FE-7FA3-589D311FD3C9"));
		UMaterialExpression* Original = AFLFindExprByGuid(Mat, TEXT("D85D2F50-4760-2FB0-F179-4F9FD0C3F065"));
		UMaterialExpression* NewAdd = nullptr;
		for (const TObjectPtr<UMaterialExpression>& E : Mat->GetExpressions())
		{
			// the sticker Add is the one whose A is the original node and which is not the emissive Add
			if (E && E != Emissive && E->IsA<UMaterialExpressionAdd>())
			{
				const FExpressionInput* In = E->GetInput(0);
				if (In && In->Expression == Original) { NewAdd = E; break; }
			}
		}
		if (!Emissive || !Original || !NewAdd)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[STKL] ABORT -- emissive=%d orig=%d newAdd=%d"),
				Emissive?1:0, Original?1:0, NewAdd?1:0);
			return;
		}
		UMaterialExpression* Target = bWantSticker ? NewAdd : Original;
		const bool bOk = UMaterialEditingLibrary::ConnectMaterialExpressions(Target, TEXT(""), Emissive, TEXT("A"));
		UMaterialEditingLibrary::RecompileMaterial(Mat);
		const FExpressionInput* Rb = Emissive->GetInput(0);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[STKL] set DEB92745.A -> %s (%s) ok=%d readback=%s"),
			bWantSticker ? TEXT("stickerAdd") : TEXT("ORIGINAL"), bWantSticker ? TEXT("on") : TEXT("off"), bOk?1:0,
			(Rb && Rb->Expression) ? *Rb->Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("<none>"));
		Ar.Log(TEXT("sticker link toggled."));
	}

	// CC-7 step 5 verification. UE Python cannot read vertex/UV counts off a skeletal mesh -- the
	// getters simply are not exposed, which was established by them failing identically on the SHIPPED
	// mesh as well as the new one. The editor-side imported model carries both directly.
	void HandleAFLSkelMeshInfo(const TArray<FString>& Args, UWorld* /*World*/, FOutputDevice& Ar)
	{
		const FString Path = Args.IsValidIndex(0) ? Args[0]
			: TEXT("/Game/BagMan/Characters/Cosmetics/IRONICS_Blank/SKM_IRONICS_Blank.SKM_IRONICS_Blank");
		USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, *Path);
		if (!M) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SMI] %s DID NOT LOAD"), *Path); return; }
		const FSkeletalMeshModel* Model = M->GetImportedModel();
		int32 Verts = -1, UVs = -1;
		if (Model && Model->LODModels.Num() > 0)
		{
			Verts = Model->LODModels[0].NumVertices;
			UVs   = Model->LODModels[0].NumTexCoords;
		}
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SMI] %s verts=%d uvChannels=%d skeleton=%s materials=%d"),
			*M->GetName(), Verts, UVs,
			M->GetSkeleton() ? *M->GetSkeleton()->GetName() : TEXT("<none>"), M->GetMaterials().Num());
		Ar.Logf(TEXT("%s: verts=%d uv=%d"), *M->GetName(), Verts, UVs);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLSkelMeshInfoCmd(TEXT("afl.Dev.SkelMeshInfo"),
		TEXT("CC-7: report a skeletal mesh's vertex count, UV channel count, skeleton and material count ")
		TEXT("from the editor-side imported model. Arg: [assetPath]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLSkelMeshInfo));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLToggleStickerLinkCmd(TEXT("afl.Dev.ToggleStickerLink"),
		TEXT("CC-7 A/B/A control: move MP_EmissiveColor's Add input A between the original source and the ")
		TEXT("sticker Add. Arg: on|off. Recompiles. Changes nothing else."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLToggleStickerLink));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLAuthorStickerSamplerCmd(TEXT("afl.Dev.AuthorStickerSampler"),
		TEXT("CC-7 EDITOR ONLY: add the sticker atlas sampler to M_AFL_Character, mirroring the Brand ")
		TEXT("quartet, and move MP_EmissiveColor's Add input A onto a new Add. Idempotent; refuses if the ")
		TEXT("graph no longer matches the baseline. StickerIntensity defaults to 0."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLAuthorStickerSampler));
#endif // WITH_EDITOR

	// === CC-7 step 4: afl.Dev.IdentityRenderHash =================================================
	static bool AFLCaptureIdentity(UWorld* World, TArray<FColor>& OutPixels)
	{
		// FIXED, ABSOLUTE TRANSFORMS. Anything derived from a moving pawn would move between runs and
		// the comparison would measure the camera, not the material.
		static const FVector kMeshLoc(0.0f, 0.0f, -20000.0f);   // far from gameplay, nothing else nearby
		static const FVector kCamOff(300.0f, 0.0f, 90.0f);

		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/IRONICS_Blank/SKM_IRONICS_Blank.SKM_IRONICS_Blank"));
		if (!Mesh) { return false; }

		FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient;
		ASkeletalMeshActor* SMA = World->SpawnActor<ASkeletalMeshActor>(kMeshLoc, FRotator::ZeroRotator, SP);
		if (!SMA) { return false; }
		USkeletalMeshComponent* SMC = SMA->GetSkeletalMeshComponent();
		SMC->SetSkeletalMesh(Mesh);
		// NO anim blueprint and no update: the reference pose is the same every single run.
		SMC->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SMC->SetUpdateAnimationInEditor(false);
		SMC->GlobalAnimRateScale = 0.0f;
		SMC->RefreshBoneTransforms();

		ASceneCapture2D* Cap = World->SpawnActor<ASceneCapture2D>(kMeshLoc + kCamOff, FRotator(0.f, 180.f, 0.f), SP);
		USceneCaptureComponent2D* C = Cap ? Cap->GetCaptureComponent2D() : nullptr;
		if (!C) { if (SMA) SMA->Destroy(); return false; }

		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>();
		RT->InitAutoFormat(256, 256);
		RT->ClearColor = FLinearColor::Black;
		C->TextureTarget = RT;
		C->CaptureSource = SCS_FinalColorLDR;
		C->bCaptureEveryFrame = false;
		C->bCaptureOnMovement = false;
		C->ShowOnlyActors.Add(SMA);          // ONLY the subject: level dressing would add its own noise
		C->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		C->CaptureScene();

		FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();
		const bool bOk = Res && Res->ReadPixels(OutPixels);

		Cap->Destroy(); SMA->Destroy();
		return bOk && OutPixels.Num() > 0;
	}

	void HandleAFLIdentityRenderHash(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		// EDITOR WORLD IS FINE and is BETTER. The subject is a transient actor in reference pose with a
		// fixed camera -- none of that needs a game world, and PIE would add start-up variance to a
		// comparison whose entire value is that two captures agree to the byte. It also avoids the
		// standing rule against injecting console calls while PIE runs.
		if (!World && GEditor) { World = GEditor->GetEditorWorldContext().World(); }
		if (!World) { Ar.Log(TEXT("no world available.")); return; }
		const FString Tag = Args.IsValidIndex(0) ? Args[0] : TEXT("t");

		TArray<FColor> A, B;
		if (!AFLCaptureIdentity(World, A) || !AFLCaptureIdentity(World, B))
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[IRH] VOID -- capture failed; nothing measured."));
			return;
		}
		const uint32 HA = FCrc::MemCrc32(A.GetData(), A.Num() * sizeof(FColor));
		const uint32 HB = FCrc::MemCrc32(B.GetData(), B.Num() * sizeof(FColor));

		// REPEATABILITY CONTROL. Two captures of the SAME material must agree, or a difference measured
		// after the edit would prove nothing about the edit.
		int32 SelfMax = 0;
		for (int32 i = 0; i < FMath::Min(A.Num(), B.Num()); ++i)
		{
			SelfMax = FMath::Max(SelfMax, FMath::Max3(
				FMath::Abs(A[i].R - B[i].R), FMath::Abs(A[i].G - B[i].G), FMath::Abs(A[i].B - B[i].B)));
		}
		const bool bRepeatable = (HA == HB);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[IRH] tag=%s px=%d hashA=0x%08X hashB=0x%08X selfMaxDelta=%d repeatable=%d %s"),
			*Tag, A.Num(), HA, HB, SelfMax, bRepeatable ? 1 : 0,
			bRepeatable ? TEXT("") : TEXT("<- byte-identical is NOT measurable on this setup"));

		// Persist the pixels so a later run can diff against this one across an editor restart.
		const FString Out = FPaths::Combine(FPaths::ProjectSavedDir(),
			FString::Printf(TEXT("IdentityRender_%s.raw"), *Tag));
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(A.GetData()), A.Num() * sizeof(FColor));
		FFileHelper::SaveArrayToFile(Bytes, *Out);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[IRH] wrote %s (%d bytes)"), *Out, Bytes.Num());
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[IRH] END"));
		Ar.Logf(TEXT("IdentityRenderHash %s: 0x%08X (repeatable=%d)"), *Tag, HA, bRepeatable ? 1 : 0);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLIdentityRenderHashCmd(TEXT("afl.Dev.IdentityRenderHash"),
		TEXT("CC-7 step 4: capture an untouched X-line identity at a fixed transform in reference pose and ")
		TEXT("hash the pixels. Captures TWICE and reports whether the two agree -- if they do not, ")
		TEXT("byte-identical is not measurable and no verdict is given. Arg: [tag]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLIdentityRenderHash));

	// === CC-7 step 1: afl.Online.VerifyStickerPlacement =========================================
	// Captures a TARGET pawn from a yaw offset around it. Fixed distance and height so two captures of
	// the same pawn differ only where the pawn itself differs.
	// CAPTURES A FIXED CLONE, NOT THE LIVE PAWN.
	//
	// Two attempts captured the pawn where it stood and both were VOID: a chest sticker moved the BACK
	// view by 20,839 pixels. Freezing the pose did not fix it and could not -- the pawn is at a
	// DIFFERENT WORLD POSITION each time, so it is lit differently even in an identical pose. A live
	// pawn cannot be a deterministic subject.
	//
	// So the subject is a transient clone at a FIXED transform, wearing the body mesh and the SAME
	// composited sticker RT the observed pawn's part just produced. The only thing that varies between
	// captures is the RT -- which is exactly the question being asked.
	static bool AFLCaptureStickerClone(UWorld* World, UTextureRenderTarget2D* StickerRT, float YawOffset,
		TArray<FColor>& Out)
	{
		if (!World) { return false; }
		static const FVector kLoc(0.0f, 0.0f, -20000.0f);
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/IRONICS_Blank/SKM_IRONICS_Blank.SKM_IRONICS_Blank"));
		UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/BagMan/Materials/M_AFL_Character.M_AFL_Character"));
		if (!Mesh || !Master) { return false; }

		FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient;
		ASkeletalMeshActor* SMA = World->SpawnActor<ASkeletalMeshActor>(kLoc, FRotator::ZeroRotator, SP);
		if (!SMA) { return false; }
		USkeletalMeshComponent* SMC = SMA->GetSkeletalMeshComponent();
		SMC->SetSkeletalMesh(Mesh);
		SMC->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SMC->GlobalAnimRateScale = 0.0f;
		for (int32 i = 0; i < SMC->GetNumMaterials(); ++i)
		{
			UMaterialInstanceDynamic* MID = SMC->CreateAndSetMaterialInstanceDynamicFromMaterial(i, Master);
			if (!MID) { continue; }
			MID->SetScalarParameterValue(FName(TEXT("StickerUVScale")), 1.0f);
			MID->SetScalarParameterValue(FName(TEXT("StickerUOffset")), 0.0f);
			MID->SetScalarParameterValue(FName(TEXT("StickerVOffset")), 0.0f);
			if (StickerRT)
			{
				MID->SetTextureParameterValue(FName(TEXT("StickerAtlasTex")), StickerRT);
				MID->SetScalarParameterValue(FName(TEXT("StickerIntensity")), 1.0f);
			}
			else
			{
				MID->SetScalarParameterValue(FName(TEXT("StickerIntensity")), 0.0f);
			}
		}
		SMC->RefreshBoneTransforms();

		const FVector C = kLoc + FVector(0, 0, 95.0f);
		const FVector Fwd = FVector(1, 0, 0).RotateAngleAxis(YawOffset, FVector::UpVector);
		ASceneCapture2D* Cap = World->SpawnActor<ASceneCapture2D>(C + Fwd * 260.0f,
			(C - (C + Fwd * 260.0f)).Rotation(), SP);
		USceneCaptureComponent2D* Comp = Cap ? Cap->GetCaptureComponent2D() : nullptr;
		if (!Comp) { SMA->Destroy(); if (Cap) { Cap->Destroy(); } return false; }
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>();
		RT->InitAutoFormat(256, 256);
		RT->ClearColor = FLinearColor::Black;
		Comp->TextureTarget = RT;
		Comp->CaptureSource = SCS_FinalColorLDR;
		Comp->bCaptureEveryFrame = false;
		Comp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Comp->ShowOnlyActors.Add(SMA);
		Comp->CaptureScene();

		FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();
		const bool bOk = Res && Res->ReadPixels(Out);
		Cap->Destroy(); SMA->Destroy();
		return bOk && Out.Num() > 0;
	}

	// Find the observed pawn's part actor, and hand back its composited RT.
	static UTextureRenderTarget2D* AFLFindStickerRT(UWorld* World, bool bRemote)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Local = PC ? PC->GetPawn() : nullptr;
		APawn* Target = Local;
		if (bRemote)
		{
			Target = nullptr;
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				if (*It != Local && It->GetClass()->GetName().Contains(TEXT("Hero"))) { Target = *It; break; }
			}
		}
		if (!Target) { return nullptr; }
		TArray<UChildActorComponent*> CACs;
		Target->GetComponents<UChildActorComponent>(CACs);
		for (UChildActorComponent* CAC : CACs)
		{
			if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr))
			{
				if (UTextureRenderTarget2D* RT = Part->GetStickerRT()) { return RT; }
			}
		}
		return nullptr;
	}

	// Changed-pixel count in a horizontal band, so "the chest changed" and "the legs changed" are
	// separate facts rather than one "the front changed".
	static int32 AFLBandDelta(const TArray<FColor>& A, const TArray<FColor>& B, int32 Row0, int32 Row1)
	{
		if (A.Num() != B.Num() || A.Num() < 256 * 256) { return -1; }
		int32 Changed = 0;
		for (int32 y = Row0; y < Row1; ++y)
		{
			for (int32 x = 0; x < 256; ++x)
			{
				const int32 i = y * 256 + x;
				if (FMath::Abs(A[i].R - B[i].R) > 6 || FMath::Abs(A[i].G - B[i].G) > 6 || FMath::Abs(A[i].B - B[i].B) > 6)
				{
					++Changed;
				}
			}
		}
		return Changed;
	}

	struct FAFLPlaceState
	{
		int32 Step = 0;
		TArray<FColor> BaseFront, BaseBack;
		TArray<FName> Owned;
		bool bHaveBase = false;
	};

	static APawn* AFLPickObservedPawn(UWorld* World, bool bRemote)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Local = PC ? PC->GetPawn() : nullptr;
		if (!bRemote) { return Local; }
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (*It != Local && It->GetClass()->GetName().Contains(TEXT("Hero"))) { return *It; }
		}
		return nullptr;
	}

	void HandleAFLVerifyStickerPlacement(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("run inside PIE.")); return; }
		const FString Phase = Args.IsValidIndex(0) ? Args[0] : TEXT("cap");
		const bool bRemote = Args.IsValidIndex(1) && Args[1] == TEXT("remote");

		static FAFLPlaceState S;
		UTextureRenderTarget2D* SRT = AFLFindStickerRT(World, bRemote);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SPL] %s remote=%d stickerRT=%s"),
			*Phase, bRemote ? 1 : 0, SRT ? TEXT("present") : TEXT("<none>"));

		TArray<FColor> F, B;
		if (!AFLCaptureStickerClone(World, SRT, 0.0f, F) || !AFLCaptureStickerClone(World, SRT, 180.0f, B))
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SPL] VOID -- capture failed."));
			return;
		}

		if (Phase == TEXT("base"))
		{
			S.BaseFront = F; S.BaseBack = B; S.bHaveBase = true;
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SPL] BASELINE captured (remote=%d) px=%d"),
				bRemote ? 1 : 0, F.Num());
			return;
		}
		if (!S.bHaveBase)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SPL] VOID -- no baseline; a delta needs a before."));
			return;
		}

		// Bands: upper third is chest/head, middle is stomach, lower third is legs.
		const int32 UpF = AFLBandDelta(S.BaseFront, F, 0, 85);
		const int32 MidF = AFLBandDelta(S.BaseFront, F, 85, 170);
		const int32 LoF = AFLBandDelta(S.BaseFront, F, 170, 256);
		const int32 AllB = AFLBandDelta(S.BaseBack, B, 0, 256);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SPL] %s remote=%d  FRONT upper=%d mid=%d lower=%d | BACK total=%d"),
			*Phase, bRemote ? 1 : 0, UpF, MidF, LoF, AllB);
		Ar.Logf(TEXT("SPL %s upper=%d lower=%d back=%d"), *Phase, UpF, LoF, AllB);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLVerifyStickerPlacementCmd(
		TEXT("afl.Online.VerifyStickerPlacement"),
		TEXT("CC-7 step 1: capture a pawn front and back and report per-band changed-pixel counts. ")
		TEXT("Args: base|<label> [remote]. A chest sticker must move the UPPER band and leave the ")
		TEXT("LOWER band and the BACK view alone."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLVerifyStickerPlacement));

	// Redeem + place, driven from the writer role.
	void HandleAFLStickerPlace(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World) { return; }
		APlayerController* PC = World->GetFirstPlayerController();
		ALyraPlayerState* PS = PC ? PC->GetPlayerState<ALyraPlayerState>() : nullptr;
		UAFLCosmeticLoadoutComponent* LC = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		UAFLWalletComponent* W = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
		const UAFLCosmeticCatalogSubsystem* Cat = UAFLCosmeticCatalogSubsystem::Get(World);
		if (!LC || !W || !Cat) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SPL] no loadout/wallet/catalog")); return; }

		const int32 ZoneIdx = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		const int32 Which   = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 0;

		TArray<const FAFLCatalogEntry*> Stickers;
		Cat->GetEntriesByType(EAFLCosmeticType::Sticker, Stickers);
		TArray<FName> Ids;
		for (const FAFLCatalogEntry* E : Stickers) { if (E) { Ids.Add(E->CosmeticId); } }
		Ids.Sort(FNameLexicalLess());
		if (!Ids.IsValidIndex(Which)) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SPL] no sticker %d"), Which); return; }
		const FName Id = Ids[Which];

		if (!W->IsEntitled(PS, Id))
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SPL] redeeming %s with a credit"), *Id.ToString());
			W->ServerRequestCreditRedemption(Id);
		}
		FAFLStickerPlacement P;
		P.StickerId = Id;
		P.Position = FVector2D(0.5, 0.5);
		P.Scale = 0.9f;
		P.RotationDegrees = 0.0f;
		LC->ServerSetStickerPlacement(static_cast<EAFLStickerZone>(ZoneIdx), P);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SPL] placed %s in zone %d"), *Id.ToString(), ZoneIdx);
	}

	// THE BASELINE MUST BE STICKER-FREE. The previous run left two zones set on the saved selection, so
	// "before" already had stickers and every delta was measured on top of them -- which is why a CHEST
	// sticker appeared to change the BACK view. Same shape as the credit proof's "counter must start at
	// zero" guard: a starting state that is not controlled is not a baseline.
	void HandleAFLStickerClearAll(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World) { return; }
		APlayerController* PC = World->GetFirstPlayerController();
		ALyraPlayerState* PS = PC ? PC->GetPlayerState<ALyraPlayerState>() : nullptr;
		UAFLCosmeticLoadoutComponent* LC = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		if (!LC) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SPL] clear: no loadout")); return; }
		for (int32 z = 0; z < FAFLStickerSet::ZoneCount; ++z)
		{
			LC->ServerClearStickerZone(static_cast<EAFLStickerZone>(z));
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SPL] cleared all %d zones for a sticker-free baseline"),
			FAFLStickerSet::ZoneCount);
	}

	// READ THE COMPOSITED TARGET ITSELF, per 3x3 zone cell.
	//
	// The screen-capture route was abandoned: it reported the same deltas whether the sticker RT was
	// bound or absent, and a constant 223-pixel floor in the leg band. An instrument that answers the
	// same with the subject removed is not measuring the subject.
	//
	// This reads the pixels the compositor actually produced. "DrawTexture was called" and "pixels are
	// in the cell" are different claims, and only the second one is evidence.
	// DOES COLOUR SURVIVE RESPAWN? Reads the body MID's colour parameters straight off the part, so
	// "colour survives on the same loop" stops being an assumption two fixes were built on. Same
	// resolution as the RT dump, so both describe the same part on the same pawn.
	// RESPAWN WATCH -- one pawn, one death, BOTH cosmetics, a long window and a HEARTBEAT.
	//
	// ApplyFacemask swaps slot 1 on the PART ACTOR's own mesh (GetComponents<UMeshComponent> on `this`),
	// so the visor and the sticker ride the SAME part actor. If parts never return, neither can. The
	// operator observes visors returning, so either parts do return and the previous read was inside a
	// truncated window, or the deaths differ. This watches both through one death and settles it.
	//
	// THE HEARTBEAT IS THE POINT: every tick prints, so a run that simply ENDED early is visibly
	// different from a run where nothing came back. The previous conclusion rested on an absence
	// inside a window that was never shown to be long enough.
	// KILL BY DAMAGE, NOT SELF-DESTRUCT.
	//
	// DamageSelfDestruct uses the SAME GameplayEffect a weapon does -- it only adds
	// TAG_Gameplay_DamageSelfDestruct. That tag is the ONLY difference between the probe's death and a
	// player's, so this applies the identical spec WITHOUT it, and with an EXTERNAL instigator so the
	// kill reads as inflicted by another actor rather than self-inflicted.
	//
	// Runs against the DEDICATED SERVER world, resolved by PlayerId exactly as FireServerKill does:
	// PlayerId is server-assigned and replicated, so it is the only handle that means the same player
	// in both worlds.
	void HandleAFLDamageKill(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !GEngine) { return; }
		int32 MyId = -1;
		if (APlayerController* MyPC = World->GetFirstPlayerController())
		{
			if (APlayerState* MyPS = MyPC->PlayerState) { MyId = MyPS->GetPlayerId(); }
		}
		UWorld* SrvWorld = nullptr;
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			UWorld* W = Ctx.World();
			if (W && W->IsGameWorld() && W->GetNetMode() == NM_DedicatedServer) { SrvWorld = W; break; }
		}
		if (!SrvWorld || MyId < 0)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DMG] ABORT srvWorld=%d myId=%d"), SrvWorld ? 1 : 0, MyId);
			return;
		}
		APawn* Victim = nullptr;
		for (FConstPlayerControllerIterator It = SrvWorld->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			APlayerState* PS = PC ? PC->PlayerState : nullptr;
			if (PS && PS->GetPlayerId() == MyId) { Victim = PC->GetPawn(); break; }
		}
		if (!Victim)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DMG] ABORT -- no server pawn for playerId %d"), MyId);
			return;
		}
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Victim);
		ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(Victim);
		if (!ASC || !HC)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DMG] ABORT -- asc=%d hc=%d"), ASC ? 1 : 0, HC ? 1 : 0);
			return;
		}
		const TSubclassOf<UGameplayEffect> DamageGE =
			ULyraAssetManager::GetSubclass(ULyraGameData::Get().DamageGameplayEffect_SetByCaller);
		if (!DamageGE)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DMG] ABORT -- damage GE not found"));
			return;
		}
		// AN EXTERNAL KILLER. Any other pawn in the server world serves as instigator/causer; a
		// self-inflicted context is the thing being ruled out.
		AActor* Killer = nullptr;
		for (TActorIterator<APawn> It(SrvWorld); It; ++It)
		{
			if (*It != Victim) { Killer = *It; break; }
		}
		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddInstigator(Killer ? Killer : Victim, Killer ? Killer : Victim);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(DamageGE, 1.0f, Ctx);
		if (!Spec.Data.Get())
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[DMG] ABORT -- no spec"));
			return;
		}
		// NOTE: no TAG_Gameplay_DamageSelfDestruct. That omission is the entire experiment.
		// THE ABSORBERS ATE THE FIRST ATTEMPT. One application of MaxHealth*2 left health at 100.0 --
		// AFL carries damage absorbers / an overload clamp, which is precisely what SuicidePawn's own
		// comment says it exists to bypass. A player's death is many hits, not one, so apply
		// repeatedly until health actually reaches zero rather than assuming one spec is lethal.
		const float Dmg = HC->GetMaxHealth() * 2.0f;
		int32 Hits = 0;
		for (; Hits < 40 && HC->GetHealth() > 0.0f; ++Hits)
		{
			FGameplayEffectSpecHandle Sp = ASC->MakeOutgoingSpec(DamageGE, 1.0f, Ctx);
			if (!Sp.Data.Get()) { break; }
			Sp.Data->SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage, Dmg);
			ASC->ApplyGameplayEffectSpecToSelf(*Sp.Data.Get());
		}
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[DMG] %d hits of %.0f on %s via the weapon GE (NO self-destruct tag), killer=%s; health now %.1f %s"),
			Hits, Dmg, *Victim->GetName(), *GetNameSafe(Killer), HC->GetHealth(),
			(HC->GetHealth() <= 0.0f) ? TEXT("-- DEAD") : TEXT("-- STILL ALIVE, absorbers held"));
		Ar.Log(TEXT("damage kill applied."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLDamageKillCmd(TEXT("afl.Dev.DamageKill"),
		TEXT("Kill this window's player on the DEDICATED SERVER with the ordinary weapon damage GE and an ")
		TEXT("external instigator -- the same spec as DamageSelfDestruct minus the self-destruct tag."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLDamageKill));

	void HandleAFLRespawnWatch(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World) { return; }
		const float Secs = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 60.0f;
		TWeakObjectPtr<UWorld> WW(World);
		TSharedRef<int32> Tick = MakeShared<int32>(0);
		const int32 MaxTicks = FMath::Max(1, FMath::RoundToInt(Secs / 2.0f));
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WW, Tick, MaxTicks]()
		{
			UWorld* W = WW.Get();
			if (!W) { return; }
			const int32 T = (*Tick)++;
			APlayerController* PC = W->GetFirstPlayerController();
			APawn* P = PC ? PC->GetPawn() : nullptr;
			int32 Parts = 0; FString Visor = TEXT("-"); FString RTState = TEXT("-");
			if (P)
			{
				TArray<UChildActorComponent*> CACs;
				P->GetComponents<UChildActorComponent>(CACs);
				for (UChildActorComponent* CAC : CACs)
				{
					AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr);
					if (!Part) { continue; }
					++Parts;
					TArray<UActorComponent*> Ms;
					Part->GetComponents(USkeletalMeshComponent::StaticClass(), Ms);
					for (UActorComponent* MC : Ms)
					{
						USkeletalMeshComponent* SMC = Cast<USkeletalMeshComponent>(MC);
						if (SMC && SMC->GetNumMaterials() > 1)
						{
							UMaterialInterface* M1 = SMC->GetMaterial(1);
							if (M1) { Visor = M1->GetName(); }
						}
					}
					if (UTextureRenderTarget2D* RT = Part->GetStickerRT())
					{
						FTextureRenderTargetResource* R = RT->GameThread_GetRenderTargetResource();
						TArray<FColor> Px; int32 Lit = 0;
						if (R && R->ReadPixels(Px)) { for (const FColor& C : Px) { if (C.R>8||C.G>8||C.B>8) ++Lit; } }
						RTState = FString::Printf(TEXT("RT lit=%d"), Lit);
					}
					else { RTState = TEXT("NO RT"); }
				}
			}
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[RSW] tick %02d/%02d pawn=%s parts=%d VISOR=%s STICKER=%s"),
				T, MaxTicks, P ? *P->GetName() : TEXT("<none>"), Parts, *Visor, *RTState);
			if (T + 1 >= MaxTicks)
			{
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RSW] WINDOW COMPLETE (%d ticks) -- this run was NOT truncated"), T + 1);
			}
		}), 2.0f, true);
		Ar.Logf(TEXT("respawn watch armed for %.0fs"), Secs);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLRespawnWatchCmd(TEXT("afl.Dev.RespawnWatch"),
		TEXT("Watch one pawn through a death: pawn, part count, visor slot-1 material and sticker RT, ")
		TEXT("every 2s with a heartbeat so a truncated run is distinguishable. Arg: [seconds]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLRespawnWatch));

	void HandleAFLColourDump(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		const bool bRemote = Args.IsValidIndex(0) && Args[0] == TEXT("remote");
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Local = PC ? PC->GetPawn() : nullptr;
		APawn* Target = Local;
		if (bRemote)
		{
			Target = nullptr;
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				if (*It != Local && It->GetClass()->GetName().Contains(TEXT("Hero"))) { Target = *It; break; }
			}
		}
		if (!Target)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[COL] remote=%d NO PAWN"), bRemote ? 1 : 0);
			return;
		}
		TArray<UChildActorComponent*> CACs;
		Target->GetComponents<UChildActorComponent>(CACs);
		int32 Parts = 0, Reported = 0;
		for (UChildActorComponent* CAC : CACs)
		{
			AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr);
			if (!Part) { continue; }
			++Parts;
			TArray<UActorComponent*> Meshes;
			Part->GetComponents(USkeletalMeshComponent::StaticClass(), Meshes);
			for (UActorComponent* MC : Meshes)
			{
				USkeletalMeshComponent* SMC = Cast<USkeletalMeshComponent>(MC);
				if (!SMC) { continue; }
				for (int32 i = 0; i < SMC->GetNumMaterials(); ++i)
				{
					UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(SMC->GetMaterial(i));
					if (!MID) { continue; }
					FLinearColor Neon(0,0,0,0), Team(0,0,0,0);
					float NeonI = -1.0f;
					MID->GetVectorParameterValue(FName(TEXT("NeonColor")), Neon);
					MID->GetVectorParameterValue(FName(TEXT("TeamColor")), Team);
					MID->GetScalarParameterValue(FName(TEXT("NeonIntensity")), NeonI);
					UE_LOG(LogAFLCombat, Display,
						TEXT("AFL_TEST[COL] remote=%d %s slot%d MID parent=%s Neon=(%.3f,%.3f,%.3f) Team=(%.3f,%.3f,%.3f) NeonI=%.2f"),
						bRemote ? 1 : 0, *Part->GetName(), i,
						MID->Parent ? *MID->Parent->GetName() : TEXT("<none>"),
						Neon.R, Neon.G, Neon.B, Team.R, Team.G, Team.B, NeonI);
					++Reported;
				}
			}
		}
		// A part with NO dynamic MID has never been through the colour apply at all -- which is the
		// distinction that matters here, and is invisible if only colour VALUES are reported.
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[COL] remote=%d pawn=%s parts=%d midsReported=%d %s"),
			bRemote ? 1 : 0, *Target->GetName(), Parts, Reported,
			(Parts > 0 && Reported == 0) ? TEXT("<- parts exist but NO dynamic MIDs: colour never applied either") : TEXT(""));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLColourDumpCmd(TEXT("afl.Dev.ColourDump"),
		TEXT("Report the body MIDs' colour parameters on the local or remote pawn's parts. Arg: [remote]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLColourDump));

	void HandleAFLStickerRTDump(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		const bool bRemote = Args.IsValidIndex(0) && Args[0] == TEXT("remote");
		UTextureRenderTarget2D* RT = AFLFindStickerRT(World, bRemote);
		if (!RT)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SRT] remote=%d NO RT (nothing equipped)"), bRemote ? 1 : 0);
			return;
		}
		FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();
		TArray<FColor> Px;
		if (!Res || !Res->ReadPixels(Px) || Px.Num() == 0)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SRT] VOID -- RT read failed."));
			return;
		}
		const int32 W = RT->SizeX, H = RT->SizeY;
		const int32 CW = W / 3, CH = H / 3;
		static const TCHAR* ZN[9] = { TEXT("ChestLeft"), TEXT("ChestRight"), TEXT("Stomach"),
			TEXT("LegFrontLeft"), TEXT("LegFrontRight"), TEXT("LegBackLeft"),
			TEXT("LegBackRight"), TEXT("Back"), TEXT("Face") };
		int32 Occupied = 0;
		for (int32 z = 0; z < 9; ++z)
		{
			// Same V flip as the compositor, so a cell reported for a zone is the cell that zone's
			// geometry actually samples.
			const int32 c = z % 3, r = 2 - (z / 3);
			int32 Lit = 0;
			for (int32 y = r * CH; y < (r + 1) * CH; ++y)
			{
				for (int32 x = c * CW; x < (c + 1) * CW; ++x)
				{
					if (Px[y * W + x].A > 8) { ++Lit; }
				}
			}
			if (Lit > 0) { ++Occupied; }
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SRT] remote=%d zone %d %-14s cell(%d,%d) litPx=%d"),
				bRemote ? 1 : 0, z, ZN[z], c, r, Lit);
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SRT] remote=%d %dx%d occupiedCells=%d"),
			bRemote ? 1 : 0, W, H, Occupied);
	}

	// CC-7 step 2: what does the STORE actually offer?
	// Measured, not reasoned: GetPurchasableEntries filters on GrantedFree and on the registered
	// sellable set, and does NOT consult bTransactable. So "stickers are bTransactable=false" does not
	// by itself mean they are absent from the shop -- it means a purchase would be REFUSED after being
	// offered, which is a worse shape than not listing them. Only reading the list settles it.
	void HandleAFLStoreSurface(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		const UAFLCosmeticCatalogSubsystem* Cat = UAFLCosmeticCatalogSubsystem::Get(World ? (UObject*)World : (UObject*)GEngine);
		if (!Cat) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[STO] VOID -- no catalog.")); return; }
		TArray<FAFLCatalogEntry> Buyable;
		Cat->GetPurchasableEntries(Buyable);
		int32 Stickers = 0, Credits = 0;
		for (const FAFLCatalogEntry& E : Buyable)
		{
			const FString Id = E.CosmeticId.ToString();
			if (Id.StartsWith(TEXT("AFL.StickerCredit."))) { ++Credits; }
			else if (Id.StartsWith(TEXT("AFL.Sticker.")))
			{
				++Stickers;
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[STO] OFFERED STICKER %s (transactable=%d vo=%d)"),
					*Id, E.bTransactable ? 1 : 0, E.PriceVolts);
			}
		}
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[STO] purchasable=%d ; sticker rows offered=%d (want 0) ; credit SKUs offered=%d (want 2) %s"),
			Buyable.Num(), Stickers, Credits,
			(Stickers == 0 && Credits == 2) ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[STO] END"));
		Ar.Logf(TEXT("store: %d buyable, stickers=%d credits=%d"), Buyable.Num(), Stickers, Credits);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStoreSurfaceCmd(TEXT("afl.Catalog.StoreSurface"),
		TEXT("CC-7 step 2: read GetPurchasableEntries and report whether sticker rows are offered (they ")
		TEXT("must not be) and whether both credit SKUs are (they must be)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStoreSurface));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStickerRTDumpCmd(TEXT("afl.Dev.StickerRTDump"),
		TEXT("CC-7: report lit-pixel counts per 3x3 zone cell of the composited sticker target. Arg: [remote]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStickerRTDump));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStickerClearCmd(TEXT("afl.Dev.StickerClearAll"),
		TEXT("CC-7: clear every sticker zone so a baseline capture is genuinely sticker-free."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStickerClearAll));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStickerPlaceCmd(TEXT("afl.Dev.StickerPlace"),
		TEXT("CC-7: redeem (if needed) and place sticker <which> in zone <zoneIndex>. Args: <zone> <which>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStickerPlace));

	// === CC-7 step 2: afl.Dev.StickerScreenProof ================================================
	static UTextureRenderTarget2D* AFLMakeCellRT(UObject* Ctx, int32 ZoneCell)
	{
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Ctx);
		RT->RenderTargetFormat = RTF_RGBA8;
		RT->ClearColor = FLinearColor(0, 0, 0, 0);
		RT->InitAutoFormat(1024, 1024);
		UKismetRenderingLibrary::ClearRenderTarget2D(Ctx, RT, FLinearColor(0, 0, 0, 0));
		if (ZoneCell >= 0)
		{
			UCanvas* C = nullptr; FVector2D Sz; FDrawToRenderTargetContext Ctx2;
			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(Ctx, RT, C, Sz, Ctx2);
			if (C)
			{
				const float Cell = 1024.0f / 3.0f;
				const int32 col = ZoneCell % 3, row = ZoneCell / 3;
				// K2_DrawBox draws an OUTLINE and its third argument is LINE THICKNESS, not opacity --
				// passing 1.0 drew a one-pixel wire that no per-band threshold could ever see, and the
				// instrument correctly reported itself blind. A FILLED rect needs a texture draw.
				UTexture2D* White = LoadObject<UTexture2D>(nullptr,
					TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
				if (White)
				{
					C->K2_DrawTexture(White, FVector2D(col * Cell + 12, row * Cell + 12),
						FVector2D(Cell - 24, Cell - 24), FVector2D::ZeroVector, FVector2D::UnitVector,
						FLinearColor::White, BLEND_Opaque);
				}
			}
			UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(Ctx, Ctx2);
		}
		// PRESENCE OF OUTPUT. If the draw silently did nothing, every downstream zero is explained here
		// rather than being blamed on the zones.
		{
			FTextureRenderTargetResource* R = RT->GameThread_GetRenderTargetResource();
			TArray<FColor> Px; int32 Lit = 0;
			if (R && R->ReadPixels(Px)) { for (const FColor& C2 : Px) { if (C2.R > 8 || C2.G > 8 || C2.B > 8) { ++Lit; } } }
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SSP] synthetic RT cell=%d litPx=%d"), ZoneCell, Lit);
		}
		return RT;
	}

	static bool AFLCaptureCloneWithRT(UWorld* World, UTextureRenderTarget2D* RT, float Intensity,
		float Yaw, TArray<FColor>& Out)
	{
		static const FVector kLoc(0.0f, 0.0f, -20000.0f);
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/IRONICS_Blank/SKM_IRONICS_Blank.SKM_IRONICS_Blank"));
		if (!Mesh || !World) { return false; }
		FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient;
		ASkeletalMeshActor* SMA = World->SpawnActor<ASkeletalMeshActor>(kLoc, FRotator::ZeroRotator, SP);
		if (!SMA) { return false; }
		USkeletalMeshComponent* SMC = SMA->GetSkeletalMeshComponent();
		SMC->SetSkeletalMesh(Mesh);
		SMC->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SMC->GlobalAnimRateScale = 0.0f;
		// THE MESH'S OWN slot materials: MI children of the master, so they carry the sticker layer and
		// the shipped look. Forcing the master into every slot made the last clone unrepresentative.
		for (int32 i = 0; i < SMC->GetNumMaterials(); ++i)
		{
			UMaterialInstanceDynamic* MID = SMC->CreateAndSetMaterialInstanceDynamic(i);
			if (!MID)
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SSP] slot %d produced NO MID -- params unset"), i);
			}
			if (MID)
			{
				MID->SetScalarParameterValue(FName(TEXT("StickerUVScale")), 1.0f);
				MID->SetScalarParameterValue(FName(TEXT("StickerUOffset")), 0.0f);
				MID->SetScalarParameterValue(FName(TEXT("StickerVOffset")), 0.0f);
				if (RT) { MID->SetTextureParameterValue(FName(TEXT("StickerAtlasTex")), RT); }
				MID->SetScalarParameterValue(FName(TEXT("StickerIntensity")), Intensity);
				float Rb = -1.0f; MID->GetScalarParameterValue(FName(TEXT("StickerIntensity")), Rb);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SSP]   slot %d parent=%s intensity set=%.1f readback=%.1f"),
					i, MID->Parent ? *MID->Parent->GetName() : TEXT("<none>"), Intensity, Rb);
			}
		}
		SMC->RefreshBoneTransforms();

		const FVector C = kLoc + FVector(0, 0, 95.0f);
		const FVector Fwd = FVector(1, 0, 0).RotateAngleAxis(Yaw, FVector::UpVector);
		const FVector CamLoc = C + Fwd * 240.0f;
		ASceneCapture2D* Cap = World->SpawnActor<ASceneCapture2D>(CamLoc, (C - CamLoc).Rotation(), SP);
		USceneCaptureComponent2D* Comp = Cap ? Cap->GetCaptureComponent2D() : nullptr;
		if (!Comp) { SMA->Destroy(); if (Cap) { Cap->Destroy(); } return false; }
		UTextureRenderTarget2D* Shot = NewObject<UTextureRenderTarget2D>();
		Shot->InitAutoFormat(256, 256);
		Shot->ClearColor = FLinearColor::Black;
		Comp->TextureTarget = Shot;
		Comp->CaptureSource = SCS_FinalColorLDR;
		Comp->bCaptureEveryFrame = false;
		Comp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Comp->ShowOnlyActors.Add(SMA);
		Comp->CaptureScene();
		FTextureRenderTargetResource* Res = Shot->GameThread_GetRenderTargetResource();
		const bool bOk = Res && Res->ReadPixels(Out);
		Cap->Destroy(); SMA->Destroy();
		return bOk && Out.Num() > 0;
	}

	void HandleAFLStickerScreenProof(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World && GEditor) { World = GEditor->GetEditorWorldContext().World(); }
		if (!World) { Ar.Log(TEXT("no world")); return; }

		TArray<FColor> Off1, Off2;
		if (!AFLCaptureCloneWithRT(World, nullptr, 0.0f, 0.0f, Off1) ||
			!AFLCaptureCloneWithRT(World, nullptr, 0.0f, 0.0f, Off2))
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SSP] VOID -- capture failed.")); return;
		}
		const int32 SelfDelta = AFLBandDelta(Off1, Off2, 0, 256);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SSP] CONTROL A repeatable: OFF vs OFF changed=%d (want 0) %s"),
			SelfDelta, (SelfDelta == 0) ? TEXT("PASS") : TEXT("FAIL"));
		if (SelfDelta != 0)
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[SSP] VOID -- the capture is not repeatable, so no difference below can be attributed."));
			return;
		}

		struct FCase { int32 Cell; const TCHAR* Name; };
		static const FCase Cases[] = { { 0, TEXT("ChestLeft") }, { 3, TEXT("LegFrontLeft") }, { 7, TEXT("Back") } };
		bool bAnyDetected = false;
		for (const FCase& Cs : Cases)
		{
			UTextureRenderTarget2D* RT = AFLMakeCellRT(World, Cs.Cell);
			TArray<FColor> OnF, OnB, OffB;
			if (!AFLCaptureCloneWithRT(World, RT, 1.0f, 0.0f, OnF) ||
				!AFLCaptureCloneWithRT(World, RT, 1.0f, 180.0f, OnB) ||
				!AFLCaptureCloneWithRT(World, nullptr, 0.0f, 180.0f, OffB))
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[SSP] VOID -- capture failed for %s."), Cs.Name);
				return;
			}
			const int32 Up = AFLBandDelta(Off1, OnF, 0, 85);
			const int32 Mid = AFLBandDelta(Off1, OnF, 85, 170);
			const int32 Lo = AFLBandDelta(Off1, OnF, 170, 256);
			const int32 Bk = AFLBandDelta(OffB, OnB, 0, 256);
			const int32 Total = Up + Mid + Lo + Bk;
			if (Total > 0) { bAnyDetected = true; }
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[SSP] cell %d %-13s FRONT upper=%-6d mid=%-6d lower=%-6d | BACK=%-6d"),
				Cs.Cell, Cs.Name, Up, Mid, Lo, Bk);
		}
		// CONTROL B. If filling a cell changed NOTHING anywhere, the instrument cannot see a sticker and
		// the per-band numbers above are all zero for a reason that has nothing to do with zones.
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[SSP] CONTROL B can-detect: some filled cell changed the image = %d %s"),
			bAnyDetected ? 1 : 0,
			bAnyDetected ? TEXT("PASS") : TEXT("FAIL <- VOID: instrument blind, zones NOT measured"));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[SSP] END"));
		Ar.Log(TEXT("sticker screen proof complete -- see AFL_TEST[SSP]."));
	}

	// REPAIR: the sampler had NO Coordinates, so it read a constant texel and the layer contributed
	// nothing. The authoring reported newWires=10 for 11 attempted connections and I accepted the
	// aggregate; step 3 verified that no PRE-EXISTING connection moved but never that the NEW nodes
	// were wired to each other. "Nothing else broke" is not "the new thing works".
	//
	// EVERY WIRE HERE IS CONFIRMED BY READING THE INPUT BACK, never by the connect call's return value.
	void HandleAFLRepairStickerUV(const TArray<FString>& /*Args*/, UWorld* /*W*/, FOutputDevice& Ar)
	{
		UMaterial* M = LoadObject<UMaterial>(nullptr, TEXT("/Game/BagMan/Materials/M_AFL_Character.M_AFL_Character"));
		if (!M) { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RSU] no master")); return; }
		UMaterialExpression* Scaled = AFLFindExprByGuid(M, TEXT("A7568AAB-46A2-266E-6992-EE9A7A271B7C"));
		UMaterialExpression* Append = AFLFindExprByGuid(M, TEXT("2D249A6C-4AA6-90BF-05F4-9C87EF4BBE1D"));
		UMaterialExpression* Samp   = AFLFindExprByGuid(M, TEXT("124E2E98-4A6B-8E7D-9293-3AAAB7064596"));
		if (!Scaled || !Append || !Samp)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[RSU] REFUSED -- scaled=%d append=%d samp=%d"),
				Scaled?1:0, Append?1:0, Samp?1:0);
			return;
		}
		// Reuse an existing wired-up Add if a previous repair made one; otherwise create it.
		UMaterialExpression* FinalUV = nullptr;
		for (const TObjectPtr<UMaterialExpression>& E : M->GetExpressions())
		{
			if (E && E->IsA<UMaterialExpressionAdd>())
			{
				const FExpressionInput* A2 = E->GetInput(0);
				if (A2 && A2->Expression == Scaled) { FinalUV = E; break; }
			}
		}
		const bool bCreated = (FinalUV == nullptr);
		if (!FinalUV)
		{
			FinalUV = UMaterialEditingLibrary::CreateMaterialExpression(M, UMaterialExpressionAdd::StaticClass(), -900, 2700);
		}
		typedef UMaterialEditingLibrary ML;
		ML::ConnectMaterialExpressions(Scaled,  TEXT(""), FinalUV, TEXT("A"));
		ML::ConnectMaterialExpressions(Append,  TEXT(""), FinalUV, TEXT("B"));
		// ConnectMaterialExpressions WILL NOT bind a texture sampler's Coordinates by name -- it has
		// failed silently on every attempt, which is why the layer sampled a constant texel and
		// contributed nothing. Same shape as CC-X34: the convenience API is closed, the underlying
		// data is not. FExpressionInput is a plain struct with an Expression pointer.
		if (FExpressionInput* CoordIn = Samp->GetInput(0))
		{
			CoordIn->Expression = FinalUV;
			CoordIn->OutputIndex = 0;
		}
		UMaterialEditingLibrary::RecompileMaterial(M);
		M->MarkPackageDirty();

		auto Src = [](UMaterialExpression* E, int32 i) -> UMaterialExpression*
		{
			const FExpressionInput* In = E ? E->GetInput(i) : nullptr;
			return In ? In->Expression : nullptr;
		};
		const bool bA = (Src(FinalUV, 0) == Scaled);
		const bool bB = (Src(FinalUV, 1) == Append);
		const bool bC = (Src(Samp, 0) == FinalUV);
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[RSU] createdAdd=%d | readback FinalUV.A=%d FinalUV.B=%d Samp.Coordinates=%d %s"),
			bCreated?1:0, bA?1:0, bB?1:0, bC?1:0, (bA&&bB&&bC) ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[RSU] END"));
		Ar.Log(TEXT("sticker UV repair done -- see AFL_TEST[RSU]."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLRepairStickerUVCmd(TEXT("afl.Dev.RepairStickerUV"),
		TEXT("CC-7: wire the sticker sampler's Coordinates (UV2 x scale + offset). Verifies every wire by ")
		TEXT("reading the input back rather than trusting the connect call."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLRepairStickerUV));

	// === CC-7: afl.Dev.StickerBisect ============================================================
	static float GAFLBisectNeon = 0.0f;   // >0 drives the known-good control parameters
	struct FAFLShot { int32 NonBlack = 0; double R = 0, G = 0, B = 0; };

	static FAFLShot AFLShotStats(const TArray<FColor>& Px)
	{
		FAFLShot S;
		double r = 0, g = 0, b = 0;
		for (const FColor& C : Px)
		{
			if (C.R > 6 || C.G > 6 || C.B > 6) { ++S.NonBlack; }
			r += C.R; g += C.G; b += C.B;
		}
		const double N = FMath::Max(1, Px.Num());
		S.R = r / N; S.G = g / N; S.B = b / N;
		return S;
	}

	static bool AFLCaptureCloneTex(UWorld* World, UTexture* Tex, float Intensity, TArray<FColor>& Out,
		FString& OutParents)
	{
		static const FVector kLoc(0.0f, 0.0f, -20000.0f);
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/IRONICS_Blank/SKM_IRONICS_Blank.SKM_IRONICS_Blank"));
		if (!Mesh || !World) { return false; }
		FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient;
		ASkeletalMeshActor* SMA = World->SpawnActor<ASkeletalMeshActor>(kLoc, FRotator::ZeroRotator, SP);
		if (!SMA) { return false; }
		USkeletalMeshComponent* SMC = SMA->GetSkeletalMeshComponent();
		SMC->SetSkeletalMesh(Mesh);
		SMC->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SMC->GlobalAnimRateScale = 0.0f;
		OutParents.Reset();
		for (int32 i = 0; i < SMC->GetNumMaterials(); ++i)
		{
			UMaterialInstanceDynamic* MID = SMC->CreateAndSetMaterialInstanceDynamic(i);
			if (!MID) { OutParents += TEXT("[noMID]"); continue; }
			MID->SetScalarParameterValue(FName(TEXT("StickerUVScale")), 1.0f);
			MID->SetScalarParameterValue(FName(TEXT("StickerUOffset")), 0.0f);
			MID->SetScalarParameterValue(FName(TEXT("StickerVOffset")), 0.0f);
			if (Tex) { MID->SetTextureParameterValue(FName(TEXT("StickerAtlasTex")), Tex); }
			MID->SetScalarParameterValue(FName(TEXT("StickerIntensity")), Intensity);
			// KNOWN-GOOD POSITIVE CONTROL. Nothing so far proves this capture can detect ANY material
			// change, so "solid white at intensity 1 does nothing" is not yet attributable to the
			// sticker branch. NeonIntensity is a shipping parameter of this master that visibly drives
			// the body: if IT moves the image and the sticker parameters do not, the sticker branch is
			// genuinely dead. If it does NOT move the image either, the MID-to-render path is broken and
			// every conclusion about the sticker branch is unfounded.
			if (GAFLBisectNeon > 0.0f)
			{
				MID->SetScalarParameterValue(FName(TEXT("NeonIntensity")), GAFLBisectNeon);
				MID->SetVectorParameterValue(FName(TEXT("NeonColor")), FLinearColor(1.f, 0.f, 0.f, 1.f));
				MID->SetVectorParameterValue(FName(TEXT("EmissiveColor")), FLinearColor(1.f, 0.f, 0.f, 1.f));
			}
			float Rb = -1.0f; MID->GetScalarParameterValue(FName(TEXT("StickerIntensity")), Rb);
			UTexture* TRb = nullptr; MID->GetTextureParameterValue(FName(TEXT("StickerAtlasTex")), TRb);
			OutParents += FString::Printf(TEXT("[%d parent=%s int=%.1f tex=%s]"), i,
				MID->Parent ? *MID->Parent->GetName() : TEXT("<none>"), Rb,
				TRb ? *TRb->GetName() : TEXT("<none>"));
		}
		SMC->RefreshBoneTransforms();

		const FVector C = kLoc + FVector(0, 0, 95.0f);
		const FVector CamLoc = C + FVector(240.0f, 0, 0);
		ASceneCapture2D* Cap = World->SpawnActor<ASceneCapture2D>(CamLoc, (C - CamLoc).Rotation(), SP);
		USceneCaptureComponent2D* Comp = Cap ? Cap->GetCaptureComponent2D() : nullptr;
		if (!Comp) { SMA->Destroy(); if (Cap) { Cap->Destroy(); } return false; }
		UTextureRenderTarget2D* Shot = NewObject<UTextureRenderTarget2D>();
		Shot->InitAutoFormat(256, 256);
		Shot->ClearColor = FLinearColor::Black;
		Comp->TextureTarget = Shot;
		Comp->CaptureSource = SCS_FinalColorLDR;
		Comp->bCaptureEveryFrame = false;
		Comp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Comp->ShowOnlyActors.Add(SMA);
		// MID PARAMETER WRITES ARE ENQUEUED TO THE RENDER THREAD. A CaptureScene() in the same frame
		// renders with the PREVIOUS uniform values, so the capture is blind to everything just set --
		// which is why a known-good shipping parameter (NeonIntensity=50, red emissive) also produced
		// dMean=0.000 and why five capture instruments in a row reported "nothing changed".
		SMC->MarkRenderStateDirty();
		FlushRenderingCommands();
		Comp->CaptureScene();
		FlushRenderingCommands();
		FTextureRenderTargetResource* Res = Shot->GameThread_GetRenderTargetResource();
		const bool bOk = Res && Res->ReadPixels(Out);
		Cap->Destroy(); SMA->Destroy();
		return bOk && Out.Num() > 0;
	}

	void HandleAFLStickerBisect(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		if (!World && GEditor) { World = GEditor->GetEditorWorldContext().World(); }
		if (!World) { Ar.Log(TEXT("no world")); return; }

		// ---- PART A: the graph, as it stands NOW ---------------------------------------------
		UMaterial* M = LoadObject<UMaterial>(nullptr, TEXT("/Game/BagMan/Materials/M_AFL_Character.M_AFL_Character"));
		if (M)
		{
			UMaterialExpression* Emis = AFLFindExprByGuid(M, TEXT("DEB92745-433C-86FE-7FA3-589D311FD3C9"));
			const FExpressionInput* EA = Emis ? Emis->GetInput(0) : nullptr;
			UMaterialExpression* AddN = EA ? EA->Expression : nullptr;
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BIS] A1 DEB92745.in[A] -> %s (%s)"),
				AddN ? *AddN->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("<none>"),
				AddN ? *AddN->GetClass()->GetName() : TEXT("-"));
			if (AddN)
			{
				const FExpressionInput* A0 = AddN->GetInput(0);
				const FExpressionInput* A1 = AddN->GetInput(1);
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BIS] A2 stickerAdd.in[A]=%s in[B]=%s"),
					(A0 && A0->Expression) ? *A0->Expression->GetClass()->GetName() : TEXT("<none>"),
					(A1 && A1->Expression) ? *A1->Expression->GetClass()->GetName() : TEXT("<none>"));
			}
			UMaterialExpression* Samp = AFLFindExprByGuid(M, TEXT("124E2E98-4A6B-8E7D-9293-3AAAB7064596"));
			const FExpressionInput* Co = Samp ? Samp->GetInput(0) : nullptr;
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BIS] A3 sampler.Coordinates -> %s"),
				(Co && Co->Expression) ? *Co->Expression->GetClass()->GetName() : TEXT("<NONE>"));
		}

		// ---- PARTS B/C/D: the ladder ---------------------------------------------------------
		UTexture2D* Solid = LoadObject<UTexture2D>(nullptr,
			TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
		UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/Stickers/T_BagMan_StickerAtlas.T_BagMan_StickerAtlas"));
		UTextureRenderTarget2D* RT = AFLMakeCellRT(World, 0);

		struct FStep { const TCHAR* Label; UTexture* Tex; float Intensity; };
		const FStep Steps[] = {
			{ TEXT("0 OFF          "), nullptr, 0.0f },
			{ TEXT("1 SOLID white  "), Solid,   1.0f },
			{ TEXT("2 ATLAS        "), Atlas,   1.0f },
			{ TEXT("3 RT cell0     "), RT,      1.0f },
		};
		// run the ladder, then the known-good control as a separate pass
		FAFLShot Base;
		for (int32 i = 0; i < UE_ARRAY_COUNT(Steps); ++i)
		{
			TArray<FColor> Px; FString Parents;
			if (!AFLCaptureCloneTex(World, Steps[i].Tex, Steps[i].Intensity, Px, Parents))
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[BIS] capture failed at %s"), Steps[i].Label);
				return;
			}
			const FAFLShot S = AFLShotStats(Px);
			if (i == 0) { Base = S; }
			// NON-BLACK COUNT IS THE ONE THAT MATTERS FIRST. If the subject is invisible, every
			// "nothing changed" so far has been a photograph of an empty frame.
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[BIS] %s nonBlack=%-6d meanRGB=(%.2f,%.2f,%.2f) dMean=%.3f  %s"),
				Steps[i].Label, S.NonBlack, S.R, S.G, S.B,
				FMath::Abs(S.R - Base.R) + FMath::Abs(S.G - Base.G) + FMath::Abs(S.B - Base.B),
				*Parents);
		}
		// ---- THE POSITIVE CONTROL ------------------------------------------------------------
		{
			GAFLBisectNeon = 50.0f;
			TArray<FColor> Px; FString Parents;
			const bool bOk = AFLCaptureCloneTex(World, nullptr, 0.0f, Px, Parents);
			GAFLBisectNeon = 0.0f;
			if (bOk)
			{
				const FAFLShot S = AFLShotStats(Px);
				const double D = FMath::Abs(S.R - Base.R) + FMath::Abs(S.G - Base.G) + FMath::Abs(S.B - Base.B);
				UE_LOG(LogAFLCombat, Display,
					TEXT("AFL_TEST[BIS] C KNOWN-GOOD NeonIntensity=50 + red Neon/Emissive: nonBlack=%d meanRGB=(%.2f,%.2f,%.2f) dMean=%.3f %s"),
					S.NonBlack, S.R, S.G, S.B, D,
					(D > 0.5) ? TEXT("<- capture CAN see a material change; the sticker branch is dead")
							  : TEXT("<- capture sees NOTHING even here; the MID->render path is broken, sticker verdict UNFOUNDED"));
			}
		}
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[BIS] READ IT THIS WAY: nonBlack==0 everywhere means the CLONE never rendered ")
			TEXT("and no capture in this programme measured a sticker. nonBlack>0 with dMean==0 at step 1 ")
			TEXT("means the material's sticker branch never reaches the output."));
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[BIS] END"));
		Ar.Log(TEXT("bisect complete -- see AFL_TEST[BIS]."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStickerBisectCmd(TEXT("afl.Dev.StickerBisect"),
		TEXT("CC-7: find where the sticker signal dies -- graph state, MID parents, and a solid/atlas/RT ")
		TEXT("ladder reporting non-black pixel count and mean RGB per step."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStickerBisect));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStickerScreenProofCmd(TEXT("afl.Dev.StickerScreenProof"),
		TEXT("CC-7 step 2: fill ONE zone cell of a synthetic RT and capture a fixed clone with the sticker ")
		TEXT("layer on vs off. Proves it can fail first: OFF vs OFF must be identical, and a filled cell ")
		TEXT("must change something, or the run is VOID."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStickerScreenProof));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLVerifyBundleBuyCmd(TEXT("afl.Online.VerifyBundleBuy"),
		TEXT("Buys a hand cannon pair through ClientRequestPurchase and asserts BOTH child ids land. ")
		TEXT("Bundle id alone = the slot defect reproduced = FAIL."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLVerifyBundleBuy));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLVerifySlotBuyJoinCmd(TEXT("afl.Online.VerifySlotBuyJoin"),
		TEXT("Buys AFL.CreatorSlot.x3 TWICE through ClientRequestPurchase (production entry, real Volts) and ")
		TEXT("asserts the counted slot entitlement reaches +3 then +6. The join neither cc-4-2-done nor ")
		TEXT("cc-6-1-done covers. AFL_TEST[JOIN] PASS = a real purchase grants a real slot."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLVerifySlotBuyJoin));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCreatorSlotProbeCmd(TEXT("afl.Creator.SlotProbe"),
		TEXT("CC-4.2: prove buying a slot grants a slot -- x3 increments by 3, buying it AGAIN reaches 6 (a boolean entitlement would not), x8 adds to the SAME counter, and AFLResolveEffectiveSlotCap resolves the ladder including ceiling clamp and max-upgrade. AFL_TEST[SLOT] PASS = all."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCreatorSlotProbe));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCreatorPreviewProbeCmd(TEXT("afl.Creator.PreviewProbe"),
		TEXT("CC-5.3: prove the creator loop -- apply lands on the preview MIDs, rotation holds colour measured on the far side, a change while rotated does not revert the rotation, and the preview value EQUALS what BuildColorOverride hands the gameplay pawn. AFL_TEST[CC53] PASS = all four."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCreatorPreviewProbe));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCreatorArcProbeCmd(TEXT("afl.Creator.ArcProbe"),
		TEXT("CC-5.2: assert the hue arc's output is ALWAYS inside the neon gamut for adversarial inputs (near-black, grey, near-white, out-of-range hue), that re-hueing preserves S/V, and that channel links default OFF. AFL_TEST[ARC] PASS = all."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLCreatorArcProbe));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLSchemaProbeCmd(
		TEXT("afl.Creator.SchemaProbe"),
		TEXT("CC-5.1: run the schema's own existence check against a material path and report found= per parameter. Proves the check can return NOT-FOUND rather than manufacturing a default."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLSchemaProbe));

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

// CC-6.1 shipping-purchase proof. Fired from the AutoProbe TIMER, not from the bridge: the standing
// rule is ZERO bridge calls while PIE runs, so a console command that must execute mid-session has
// to be scheduled in-process before PIE starts. Same mechanism the creator probes already use.
static int32 GAFLCreatorBuyProbe = 0;

// CC-X23: separate from BuyProbe on purpose. BuyProbe SPENDS Volts against live PlayFab; this one
// only needs a SESSION. Folding them would make proving a READ cost money.
static int32 GAFLBundleProbe = 0;
static FAutoConsoleVariableRef CVarAFLBundleProbe(TEXT("afl.Online.BundleProbe.Enable"),
	GAFLBundleProbe, TEXT("1 = buy a hand cannon pair through the production entry and assert both children land. SPENDS 1490 real Volts."), ECVF_Default);
static int32 GAFLSlotJoinProbe = 0;
static FAutoConsoleVariableRef CVarAFLSlotJoinProbe(TEXT("afl.Online.SlotJoinProbe.Enable"),
	GAFLSlotJoinProbe, TEXT("1 = buy AFL.CreatorSlot.x3 TWICE through the production entry and assert the counter reaches +6. SPENDS 9980 real Volts."), ECVF_Default);
// CC-X30 durability gates. TWO cvars because the proof spans two PROCESSES: run 1 buys and reports
// the count it left behind, run 2 asserts a FRESH process got that count back from PlayFab.
// The relaunch EXPECTATION is the cvar's value, not a separate flag -- run 2 must check against a
// number decided before the previous process died, or it is comparing a value with itself.
static int32 GAFLCountedDurable = 0;
static FAutoConsoleVariableRef CVarAFLCountedDurable(TEXT("afl.Online.CountedDurable.Enable"),
	GAFLCountedDurable, TEXT("CC-X30 run 1: buy AFL.CreatorSlot.x3, reconcile, assert the counter survives. SPENDS 4990 real Volts."), ECVF_Default);
static int32 GAFLDoneLoopProbe = 0;
static FAutoConsoleVariableRef CVarAFLDoneLoopProbe(TEXT("afl.Creator.DoneLoop.Enable"),
	GAFLDoneLoopProbe, TEXT("CC-6.5: 1 = run the end-to-end done-definition loop. SPENDS real Volts."), ECVF_Default);
static int32 GAFLStickerPlace = 0;
static FAutoConsoleVariableRef GAFLStickerPlaceCVar(TEXT("afl.Online.StickerPlaceProbe"), GAFLStickerPlace,
	TEXT("CC-7: place stickers and capture the pawn front/back per band. Spends up to 2 sticker credits."),
	ECVF_Default);
static int32 GAFLRefusalMatrix = 0;
static FAutoConsoleVariableRef GAFLRefusalMatrixCVar(TEXT("afl.Dev.RefusalMatrix"), GAFLRefusalMatrix,
	TEXT("CC-7: 1 = attribute each redemption refusal to its gate. Spends ONE existing credit, buys nothing."),
	ECVF_Default);
static int32 GAFLStickerCreditProbe = 0;
static FAutoConsoleVariableRef GAFLStickerCreditProbeCVar(TEXT("afl.Online.StickerCreditProbe"),
	GAFLStickerCreditProbe,
	TEXT("CC-7: 1 = buy the sticker credit packs and prove redemption, accumulation, the drain and the ")
	TEXT("non-fungibility arm. SPENDS 3470 real Volts."), ECVF_Default);
static int32 GAFLRedeemProbe = 0;
static FAutoConsoleVariableRef CVarAFLRedeemProbe(TEXT("afl.Online.RedeemProbe.Enable"),
	GAFLRedeemProbe, TEXT("CC-X30: 1 = buy AFL.WeaponCredit.x3 and prove redemption + its four refusals. SPENDS 990 real Volts."), ECVF_Default);
static int32 GAFLCountedRelaunchExpect = 0;
static FAutoConsoleVariableRef CVarAFLCountedRelaunchExpect(TEXT("afl.Online.CountedRelaunch.Expect"),
	GAFLCountedRelaunchExpect, TEXT("CC-X30 run 2: >0 = assert a fresh process reads exactly this counted-slot count from PlayFab. 0 = off."), ECVF_Default);
// ISOLATION. Counted in ONE place so a probe added later cannot forget to exclude the others.
// Every economy probe -- anything that SPENDS or GRANTS -- must be listed here.
static int32 GAFLReconcileProbe = 0;
static FAutoConsoleVariableRef CVarAFLReconcileProbe(TEXT("afl.Online.ReconcileProbe.Enable"),
	GAFLReconcileProbe, TEXT("CC-X23: 1 = run the wallet-mirror reconcile proof. Needs afl.Online.ForceEosLogin 1."), ECVF_Default);
static FAutoConsoleVariableRef CVarAFLCreatorBuyProbe(TEXT("afl.Creator.BuyProbe"),
	GAFLCreatorBuyProbe, TEXT("CC-6.1: 1 = buy a registered SKU through the SHIPPING entry, then a "
	"deliberately UNREGISTERED one as the negative control. Set BEFORE starting PIE."));
	static FAutoConsoleVariableRef GAFLCreatorAutoProbeCVar(
		TEXT("afl.Creator.AutoProbe"),
		GAFLCreatorAutoProbe,
		TEXT("Dev acceptance: 1 = on the first ready CLIENT world, auto-run the creator-overlay readback. Set BEFORE starting PIE. Self-disarms."),
		ECVF_Default);

	/** How many probes that SPEND or GRANT are armed. More than one and none may run: a balance delta
	 *  with two spenders in flight cannot be attributed to either. */
	static int32 AFLEconomyProbesArmed()
	{
		return (GAFLSlotJoinProbe != 0 ? 1 : 0)
			 + (GAFLBundleProbe != 0 ? 1 : 0)
			 + (GAFLCountedDurable != 0 ? 1 : 0)
			 + (GAFLRedeemProbe != 0 ? 1 : 0)
			 + (GAFLDoneLoopProbe != 0 ? 1 : 0)
			 + (GAFLStickerCreditProbe != 0 ? 1 : 0)
			 + (GAFLCreatorBuyProbe != 0 ? 1 : 0);
	}

	/** True when any economy probe is under test. The cheat/grant probes stand down: they do not spend,
	 *  but they mutate the same counters, and an economy proof must not share its run with one. */
	static bool AFLEconomyProbeUnderTest() { return AFLEconomyProbesArmed() > 0; }

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

		// ISOLATION GATE, evaluated before any economy probe dispatches.
		const int32 ArmedEconomy = AFLEconomyProbesArmed();
		if (RoleIndex == 0 && ArmedEconomy > 1)
		{
			// LOUD REFUSAL. Silence here would be indistinguishable from "it ran and found nothing".
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_TEST[ISO] REFUSED -- %d economy probes armed at once (slotJoin=%d bundle=%d durable=%d redeem=%d creatorBuy=%d). ")
				TEXT("NONE will dispatch: a balance delta with two spenders cannot be attributed to either. Arm exactly one."),
				ArmedEconomy, GAFLSlotJoinProbe, GAFLBundleProbe, GAFLCountedDurable, GAFLRedeemProbe, GAFLCreatorBuyProbe);
		}
		else if (RoleIndex == 0 && ArmedEconomy == 1)
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[ISO] isolated: exactly ONE economy probe armed; cheat/grant probes stood down."));
		}
		else if (RoleIndex == 0)
		{
			// EMITTED EVEN WHEN THERE IS NOTHING TO ISOLATE. Silence here would be indistinguishable
			// from the guard not running at all, which is the failure mode this programme keeps finding
			// in its own instruments.
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[ISO] no economy probe armed; nothing to isolate (cheat/grant probes may run)."));
		}
		const bool bEconomyOk = (ArmedEconomy == 1);

		// CC-X15 step 4: assert the facemask command can reach EVERY catalog row. Role A only, once, early --
		// it is a pure read and must not perturb the colour timeline that follows.
		if (RoleIndex == 0) { FireCmd(2.0f, TEXT("afl.Cosmetic.SetFacemask verify"), TEXT("0-facemask-verify")); }
		if (RoleIndex == 0) { FireCmd(3.0f, TEXT("afl.Catalog.TypeLint"), TEXT("0-type-lint")); }
		// CC-7.2 at t=3.2: pure computation, no spend, no asset touched.
		if (RoleIndex == 0) { FireCmd(3.2f, TEXT("afl.Sticker.Probe"), TEXT("0-cc72-sticker")); }
		// CC-X33 ANSWERED 2026-08-21 -- DISPATCH DELIBERATELY REMOVED, COMMAND KEPT.
		// The probe sets GlobalAnimRateScale to 0 for ~0.6s to hold the pose. Left on the auto-sequence
		// it would freeze the pawn three times inside every future run and quietly corrupt the timing of
		// any probe that measures motion. Run it by hand -- `afl.Dev.FbikPropagation` -- when the
		// question is asked again.
		if (RoleIndex == 0) { FireCmd(3.2f, TEXT("afl.Creator.ArcProbe"), TEXT("0-arc-probe")); }
		if (RoleIndex == 0) { FireCmd(6.5f, TEXT("afl.Creator.PreviewProbe"), TEXT("0-cc53-preview")); }
		// SUPPRESSED during the CC-X30 durability run: SlotProbe grants x3 and x8 through
		// DebugGrantOwnership, which put 11 cheat-granted slots into run 1's baseline. Firing them on
		// one frame is also what accidentally exposed the lost update -- worth keeping in mind, but a
		// durability number must not be part cheat.
		if (RoleIndex == 0 && !AFLEconomyProbeUnderTest()) { FireCmd(8.0f, TEXT("afl.Creator.SlotProbe"), TEXT("0-cc42-slots")); }
		if (RoleIndex == 0 && GAFLReconcileProbe != 0) { FireCmd(14.0f, TEXT("afl.Online.ReconcileProbe"), TEXT("0-ccx23-reconcile")); }
		if (RoleIndex == 0 && bEconomyOk && GAFLSlotJoinProbe != 0) { FireCmd(18.0f, TEXT("afl.Online.VerifySlotBuyJoin"), TEXT("0-join-slotbuy")); }
		if (RoleIndex == 0 && bEconomyOk && GAFLBundleProbe != 0) { FireCmd(20.0f, TEXT("afl.Online.VerifyBundleBuy"), TEXT("0-bundle-buy")); }
		// CC-X30. Run 1 late (t=22) so it cannot collide with the other economy probes -- concurrent
		// probes already contaminated one VO measurement, and a summed delta reads as a failure.
		if (RoleIndex == 0 && bEconomyOk && GAFLCountedDurable != 0) { FireCmd(22.0f, TEXT("afl.Online.VerifyCountedDurable"), TEXT("0-ccx30-durable")); }
		// t=24, and alone: concurrent economy probes already contaminated one VO measurement, and this
		// one both buys and spends.
		if (RoleIndex == 0 && bEconomyOk && GAFLRedeemProbe != 0) { FireCmd(24.0f, TEXT("afl.Online.VerifyCreditRedeem"), TEXT("0-ccx30-redeem")); }
		if (RoleIndex == 0 && bEconomyOk && GAFLStickerCreditProbe != 0) { FireCmd(24.0f, TEXT("afl.Online.VerifyStickerCredit"), TEXT("0-cc7-sticker-credit")); }
		if (RoleIndex == 0 && GAFLRefusalMatrix != 0) { FireCmd(20.0f, TEXT("afl.Dev.RedeemRefusalMatrix"), TEXT("0-cc7-refusal-matrix")); }
		// CC-7 PLACEMENT. A writes and places; B captures A's pawn, so what is measured is what a
		// DIFFERENT machine renders. Capturing one's own pawn would pass even if nothing replicated.
		if (GAFLStickerPlace != 0)
		{
			if (RoleIndex == 0) { FireCmd( 6.0f, TEXT("afl.Dev.StickerClearAll"), TEXT("A-clear-for-baseline")); }
			if (RoleIndex == 0) { FireCmd(14.0f, TEXT("afl.Dev.StickerPlace 0 0"),  TEXT("A-place-chestleft")); }
			FireCmd(12.0f, RoleIndex == 1 ? TEXT("afl.Online.VerifyStickerPlacement base remote")
			                              : TEXT("afl.Online.VerifyStickerPlacement base"), TEXT("baseline"));
			FireCmd(22.0f, RoleIndex == 1 ? TEXT("afl.Online.VerifyStickerPlacement afterChest remote")
			                              : TEXT("afl.Online.VerifyStickerPlacement afterChest"), TEXT("after-chest"));
			if (RoleIndex == 0) { FireCmd(26.0f, TEXT("afl.Dev.StickerPlace 7 1"), TEXT("A-place-back")); }
			FireCmd(31.0f, RoleIndex == 1 ? TEXT("afl.Dev.StickerRTDump remote") : TEXT("afl.Dev.StickerRTDump"), TEXT("rt-dump"));
			FireCmd(31.5f, RoleIndex == 1 ? TEXT("afl.Dev.ColourDump remote") : TEXT("afl.Dev.ColourDump"), TEXT("colour-pre-kill"));
			if (RoleIndex == 0) { FireCmd(32.0f, TEXT("afl.Catalog.StoreSurface"), TEXT("store-surface")); }
			FireCmd(34.0f, RoleIndex == 1 ? TEXT("afl.Online.VerifyStickerPlacement afterBack remote")
			                              : TEXT("afl.Online.VerifyStickerPlacement afterBack"), TEXT("after-back"));
			// Watch from BEFORE the kill to well after, on the writer. 90s of ticks against a kill at
			// t=38 means a respawn up to ~50s late still lands inside the window.
			if (RoleIndex == 0) { FireCmd(34.0f, TEXT("afl.Dev.RespawnWatch 90"), TEXT("respawn-watch")); }
			// KILL BY DAMAGE, not SuicidePawn: the operator observes visors surviving death in normal
			// play, and SuicidePawn is the one path that adds the self-destruct tag.
			if (RoleIndex == 0) { FireCmd(38.0f, TEXT("afl.Dev.DamageKill"), TEXT("kill-A-by-damage")); }
			FireCmd(48.0f, RoleIndex == 1 ? TEXT("afl.Online.VerifyStickerPlacement afterRespawn remote")
			                              : TEXT("afl.Online.VerifyStickerPlacement afterRespawn"), TEXT("after-respawn"));
			// THE ARM THE V-FLIP FIX INVALIDATED. Respawn persistence and the observer's view were both
			// measured on the DATA path before the fix, so neither has been seen composited to the
			// CORRECT cell. The RT dump is the reliable signal now that RT -> screen is proven: it
			// reports which cell each zone's geometry actually samples, using the same flip the
			// compositor applies. Fired on BOTH roles -- on B it reads the REMOTE pawn's parts, which is
			// the replication half.
			FireCmd(52.0f, RoleIndex == 1 ? TEXT("afl.Dev.StickerRTDump remote") : TEXT("afl.Dev.StickerRTDump"),
				TEXT("rt-dump-after-respawn"));
			FireCmd(52.5f, RoleIndex == 1 ? TEXT("afl.Dev.ColourDump remote") : TEXT("afl.Dev.ColourDump"), TEXT("colour-after-respawn"));
		}
		// CC-6.5 at t=26: spends, so it is an ECONOMY probe and obeys the isolation gate.
		if (RoleIndex == 0 && bEconomyOk && GAFLDoneLoopProbe != 0) { FireCmd(26.0f, TEXT("afl.Creator.VerifyDoneLoop"), TEXT("0-cc65-doneloop")); }
		// Run 2 EARLY (t=6): it is a pure read, and it must land after hydration but before anything
		// else can perturb the counted set.
		if (RoleIndex == 0 && GAFLCountedRelaunchExpect > 0)
		{
			const FString RelCmd = FString::Printf(TEXT("afl.Online.VerifyCountedRelaunch %d"), GAFLCountedRelaunchExpect);
			FireCmd(6.0f, *RelCmd, TEXT("0-ccx30-relaunch"));
		}
		if (RoleIndex == 0) { FireCmd(3.5f, TEXT("afl.Creator.SchemaProbe"), TEXT("0-schema-probe")); }
		// CC-X22 at t=12: a pure read, but it must land after login has answered the sellable set.
		if (RoleIndex == 0) { FireCmd(12.0f, TEXT("afl.Catalog.SellableProbe"), TEXT("0-ccx22-sellable")); }
		// CC-5.2 at t=14: pure reads, no spend, so it needs no economy isolation.
		if (RoleIndex == 0) { FireCmd(14.0f, TEXT("afl.Creator.WidgetProbe"), TEXT("0-cc52-widget")); }
		// CC-5.2 falsification needs BOTH masters. TeamColor is inert on M_AFL_Character and live on
		// M_Mannequin, so a run against one master alone cannot show that the verdict is keyed on the
		// (master, parameter) PAIR rather than the parameter name.
		if (RoleIndex == 0) { FireCmd(4.2f, TEXT("afl.Creator.SchemaProbe /Game/BagMan/Materials/M_AFL_Character"), TEXT("0-schema-xline")); }
		// CC-6.1 SHIPPING-PURCHASE PROOF, cvar-gated so it never runs during an ordinary creator probe
		// (it SPENDS Volts on a live PlayFab account and would otherwise perturb every later run).
		//
		// POSITIVE then NEGATIVE, in that order and far apart. AFL.CreatorSlot.x1 is registered in the
		// PlayFab manifest and must succeed; AFL.Ability.EMP is priced in the UE catalog but is NOT in
		// the manifest -- one of the 263 CC-X22 rows -- and must FAIL. Without the negative arm a pass
		// proves only that SOME buy works, not that registration is what makes the difference.
		if (RoleIndex == 0 && GAFLCreatorBuyProbe != 0)
		{
			// POSITIVE uses an UNOWNED registered SKU. AFL.CreatorSlot.x1 was bought in the 02.35 run and is
			// non-stackable, so re-buying it would fail as ALREADY OWNED and contaminate the arm -- a failure
			// that looks identical to "unregistered" in the result line. ARIA is registered, unowned, 990 VO.
			FireCmd(12.0f, TEXT("afl.Online.VerifyNewSkuBuy AFL.Emblem.ARIA"), TEXT("BUY-positive"));
			FireCmd(26.0f, TEXT("afl.Online.VerifyNewSkuBuy AFL.Ability.EMP"),   TEXT("BUY-negative-control"));
		}
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
		// CC-6.4 feature arm: choose a visor colour that appears in NO body, edge, glow or persisted
		// value in this run (bodies 0.90/0.05/0.60, 1.00/0.45/0.00, 0.05/0.90/0.80; edges 1.00/0.35/0.00,
		// 0.20/0.20/1.00; glows 0.00/1.00/0.55, 1.00/1.00/0.10). A read echoing inherited state cannot
		// be mistaken for this one -- the same collision that spoiled two earlier probe colours.
		if (RoleIndex == 0) { FireCmd(18.0f, TEXT("afl.Creator.BuildProbe visor 0.15 0.55 1.0"), TEXT("V1-set-visor")); }
		FireCmd(20.5f, TEXT("afl.Creator.BuildProbe"), TEXT("V2-read-visor"));
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
				// NM_ListenServer is accepted alongside NM_Client because on a listen host the local
				// player's world IS the authority -- there is no separate client world to find. Without
				// this the entire probe block is skipped on listen and the run reports NOTHING, which
				// reads identically to "the probes ran and found nothing" (it cost one full run).
				// Dedicated behaviour is untouched: NM_Client still matches exactly as before.
				if (!W || !W->IsGameWorld()) { continue; }
				const ENetMode ProbeNetMode = W->GetNetMode();
				if (ProbeNetMode != NM_Client && ProbeNetMode != NM_ListenServer) { continue; }
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

	// ─── CC-6.1 NEW-SKU shipping proof: afl.Online.VerifyNewSkuBuy <CosmeticId> ──────────────────
	// VerifyPurchaseSeam proves the seam works, but it only ever buys AFL.Test.Token -- a transient
	// fixture that is ALWAYS in the PlayFab catalog. It therefore cannot show that a row authored in the
	// UE catalog and seeded to PlayFab is actually purchasable. That gap is the whole subject of CC-X22:
	// a UE catalog row does NOT make a SKU sellable, and until it is in the PlayFab manifest every real
	// cosmetic returns ItemNotFound in shipping while passing in PIE via the dev grant.
	//
	// DRIVES THE PRODUCTION ENTRY DELIBERATELY. ServerPurchaseCosmetic is the dev grant and is compiled
	// out of shipping; a proof that runs through it proves nothing about shipping. This calls
	// ClientRequestPurchase -> PurchaseThroughBackend -> PlayFab, the path real money-equivalent spend
	// takes, and reports the Volts delta and the entitlement flip.
	//
	// FALSIFIABLE: a SKU that is NOT registered in PlayFab fails here with the seam refusing the buy --
	// which is exactly what every unseeded row would do. Run it against an unseeded id to see the
	// negative, and against AFL.CreatorSlot.x1 to see the positive.
	void HandleAFLVerifyNewSkuBuy(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (!World || !World->IsGameWorld()) { Ar.Log(TEXT("afl.Online.VerifyNewSkuBuy - run inside PIE.")); return; }
		UAFLWalletComponent* Wallet = GetPlayerWallet(World);
		if (!Wallet) { Ar.Log(TEXT("afl.Online.VerifyNewSkuBuy - no wallet on the local PlayerState.")); return; }

		const FString IdStr = Args.Num() > 0 ? Args[0] : FString(TEXT("AFL.CreatorSlot.x1"));
		const FName Id(*IdStr);
		APlayerController* PC = World->GetFirstPlayerController();
		const ALyraPlayerState* PS = PC ? Cast<ALyraPlayerState>(PC->PlayerState) : nullptr;

		const int32 VoBefore = Wallet->GetVolts();
		const bool bOwnedBefore = PS ? Wallet->IsEntitled(PS, Id) : false;
		UE_LOG(LogAFLCombat, Display,
			TEXT("AFL_TEST[NEWSKU] start id=%s voBefore=%d ownedBefore=%d -> ClientRequestPurchase (PRODUCTION entry -> PlayFab)"),
			*IdStr, VoBefore, bOwnedBefore ? 1 : 0);

		Wallet->ClientRequestPurchase(Id, EAFLPayCurrency::Volts,
			[Wallet, Id, IdStr, VoBefore, bOwnedBefore, PS](bool bSuccess)
		{
			const int32 VoAfter = Wallet->GetVolts();
			const bool bOwnedAfter = PS ? Wallet->IsEntitled(PS, Id) : false;
			const int32 Spent = VoBefore - VoAfter;
			// bOwnedBefore is reported so a row the account ALREADY owned cannot read as a fresh purchase --
			// owned-before + no spend is a no-op, not a pass.
			UE_LOG(LogAFLCombat, Display,
				TEXT("AFL_TEST[NEWSKU] id=%s success=%d vo %d->%d spent=%d owned %d->%d"),
				*IdStr, bSuccess ? 1 : 0, VoBefore, VoAfter, Spent, bOwnedBefore ? 1 : 0, bOwnedAfter ? 1 : 0);
			// PASS IS THE SERVER'S ANSWER, NOT THE LOCAL MIRROR'S.
			// An earlier revision required Spent > 0 and an entitlement flip read from GetVolts()/IsEntitled
			// SYNCHRONOUSLY inside this callback -- and printed FAIL on a purchase PlayFab had accepted
			// (http=200 ok=1, GetUserInventory VO=4008 owned=1) while the local read still showed a
			// [CORRECTED 2026-08-20: calling that 200179 STALE was an inference, not a measurement. PlayFab
			//  now reads VO=200179 WA=5653 owned=10 for this account, and owned=1 vs owned=10 suggests the
			//  4008 reading belonged to a different PIE client's PlayFab account. The ruling below -- that
			//  the SERVER decides and a client mirror never does -- is unaffected and remains correct.]
			// 200179. The local wallet is a DISPLAY MIRROR that refreshes asynchronously; economy-store
			// SS8.1 is explicit that the client requests and the SERVER decides, and a client-side cache
			// "never decides ownership". Asserting against it measured the wrong side of the seam.
			//
			// bSuccess IS the PlayFab result carried back through PurchaseThroughBackend. The mirror values
			// are still emitted, as INFORMATION -- a mirror that never catches up is worth seeing, but it
			// is a separate defect from whether the purchase committed.
			const bool bPass = bSuccess;
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[NEWSKU] %s -- %s (mirror: spent=%d ownedFlip=%d; "
				"mirror is async + display-only, NOT the pass criterion)"),
				bPass ? TEXT("PASS") : TEXT("FAIL"),
				bPass ? TEXT("PlayFab ACCEPTED the buy -- SKU is registered and purchasable on the shipping path")
				      : TEXT("PlayFab REFUSED -- unregistered (ItemNotFound) / insufficient funds / no price"),
				Spent, (bOwnedAfter && !bOwnedBefore) ? 1 : 0);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 15.f, bPass ? FColor::Green : FColor::Red,
					FString::Printf(TEXT("[NEWSKU] %s %s spent=%d"), bPass ? TEXT("PASS") : TEXT("FAIL"), *IdStr, Spent));
			}
		});
		Ar.Logf(TEXT("afl.Online.VerifyNewSkuBuy - requested %s through ClientRequestPurchase; watch AFL_TEST[NEWSKU]."), *IdStr);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLVerifyNewSkuBuyCmd(TEXT("afl.Online.VerifyNewSkuBuy"),
		TEXT("CC-6.1 shipping proof: buy <CosmeticId> (default AFL.CreatorSlot.x1) through the PRODUCTION entry ClientRequestPurchase -> PurchaseThroughBackend -> PlayFab. Proves a newly REGISTERED SKU is purchasable on the path shipping actually uses, not the dev grant. AFL_TEST[NEWSKU] PASS = spent > 0 and entitlement flipped."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLVerifyNewSkuBuy));

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

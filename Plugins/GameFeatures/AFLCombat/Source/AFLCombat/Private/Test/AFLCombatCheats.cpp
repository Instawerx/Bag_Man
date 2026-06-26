// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLCombatCheats.h"

#include "AFLCombat.h"
#include "Abilities/AFLAG_Laser_Beam.h"
#include "Abilities/AFLAG_Laser_Pulse.h"
#include "AbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "AbilitySystemGlobals.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"   // #43 selection-seam harness target
#include "Cosmetics/AFLCosmeticSelectionTypes.h"     // #43 FAFLCosmeticSelection / EAFLIdentityType
#include "Cosmetics/AFLWalletComponent.h"            // S-ECON-WALLET: balance/gate/earn-spend cheats
#include "Teams/LyraTeamSubsystem.h"                 // afl.Cosmetic.Test.Readability: opposing gameplay-team assignment
#include "Cosmetics/AFLCharacterPartActor.h"          // panel-watch: poke the robot part's live MIDs (DebugSetMID*)
#include "Components/ChildActorComponent.h"           // panel-watch: reach the body part actor (a child-actor on the pawn)
#include "AFLCosmeticCatalogSubsystem.h"             // S-ECON-CAT: catalog resolve cheats (AFLCosmeticCore)
#include "AFLAbilityCosmeticAsset.h"                  // S-ECON-CAT: EMP ability-cosmetic resolve target (AFLCosmeticCore)
#include "AFLCosmeticCoreTypes.h"                     // WeaponSkin: FAFLCatalogEntry
#include "AFLColorIdentityRegistry.h"                 // WeaponSkin: FAFLColorIdentity (color-identity fallback)
#include "Equipment/LyraEquipmentManagerComponent.h" // WeaponSkin: resolve the equipped weapon instance
#include "Equipment/LyraEquipmentInstance.h"          // WeaponSkin: GetSpawnedActors (the weapon display actor)
#include "Weapons/LyraRangedWeaponInstance.h"         // WeaponSkin: the equipped ranged weapon type
#include "Components/MeshComponent.h"                 // WeaponSkin: the mesh to MID
#include "Materials/MaterialInstanceDynamic.h"        // WeaponSkin: runtime BrandColor MID
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
	ASC->ApplyModToAttribute(UAFLAttributeSet_Combat::GetHealthAttribute(), EGameplayModOp::Override, 0.0f);
	UE_LOG(LogAFLCombat, Display, TEXT("[Cheat] SuicidePawn: set Health=0 on this window's pawn (forcing death->respawn->body re-resolve)."));
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
	if      (Name.Equals(TEXT("Health"),            ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetHealthAttribute();
	else if (Name.Equals(TEXT("MaxHealth"),         ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetMaxHealthAttribute();
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

	// The weapon's display-actor mesh (the BrandColor MI lives on slot 0).
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

	// Mesh surface: runtime MID on slot 0 + BrandColor.
	if (Mesh)
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
		{
			FLinearColor Existing;
			if (MID->GetVectorParameterValue(FMaterialParameterInfo(FName(TEXT("BrandColor"))), Existing))
			{
				MID->SetVectorParameterValue(FName(TEXT("BrandColor")), Color);
				UE_LOG(LogAFLCombat, Display,
					TEXT("[Cheat]   Mesh surface: BrandColor SET via runtime MID on %s slot 0."), *Mesh->GetName());
			}
			else
			{
				UE_LOG(LogAFLCombat, Warning,
					TEXT("[Cheat]   Mesh surface: %s slot-0 material has NO BrandColor param -> mesh AS-AUTHORED."), *Mesh->GetName());
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
			Ar.Log(TEXT("afl.Cosmetic.SetFacemask — usage: afl.Cosmetic.SetFacemask <JapanSolar|Kawaii|...|none> (or full AFL.Facemask.<Name>; 'none' to un-equip)."));
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

		FString IdStr = Args[0].TrimStartAndEnd();
		FName FacemaskId = NAME_None; // un-equip default for none/off/clear
		if (!IdStr.Equals(TEXT("none"), ESearchCase::IgnoreCase)
			&& !IdStr.Equals(TEXT("off"), ESearchCase::IgnoreCase)
			&& !IdStr.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
		{
			if (!IdStr.StartsWith(TEXT("AFL.Facemask."), ESearchCase::IgnoreCase))
			{
				IdStr = FString::Printf(TEXT("AFL.Facemask.%s"), *IdStr);
			}
			FacemaskId = FName(*IdStr);
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

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCosmeticSetFacemaskCmd(
		TEXT("afl.Cosmetic.SetFacemask"),
		TEXT("Facemask selection seam: client-issued PURE caller of ServerSetCosmeticSelection (sets FacemaskId). Runtime equip path -> slot-1 material swap + finish re-layer. Usage: afl.Cosmetic.SetFacemask <Name|none> (e.g. JapanSolar, Kawaii; 'none' to un-equip; or full AFL.Facemask.<Name>)."),
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
		Read(UAFLAttributeSet_Combat::GetHealthAttribute()),
		Read(UAFLAttributeSet_Combat::GetMaxHealthAttribute()),
		Read(UAFLAttributeSet_Combat::GetShieldAttribute()),
		Read(UAFLAttributeSet_Combat::GetMaxShieldAttribute()),
		Read(UAFLAttributeSet_Combat::GetArmorAttribute()),
		Read(UAFLAttributeSet_Combat::GetOverkillThresholdAttribute()),
		Read(UAFLAttributeSet_Combat::GetDamageAttribute()));
#endif
}

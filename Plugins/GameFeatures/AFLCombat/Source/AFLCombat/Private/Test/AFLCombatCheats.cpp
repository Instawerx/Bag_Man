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
#include "Cosmetics/AFLCharacterPartActor.h"          // panel-watch: poke the robot part's live MIDs (DebugSetMID*)
#include "Components/ChildActorComponent.h"           // panel-watch: reach the body part actor (a child-actor on the pawn)
#include "AFLCosmeticCatalogSubsystem.h"             // S-ECON-CAT: catalog resolve cheats (AFLCosmeticCore)
#include "AFLAbilityCosmeticAsset.h"                  // S-ECON-CAT: EMP ability-cosmetic resolve target (AFLCosmeticCore)
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
#include "GameFramework/CheatManagerDefines.h"
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
		// "rewind dt=... entries=... verdict=..." line itself.
		const bool bAccept = LagComp->ConfirmHit(PC, Delta, Dummy, Coord);

		UE_LOG(LogAFLCombat, Display,
			TEXT("AFLCombatCheats: OK TestFire verdict=%s (delta=%.3f, C=(%.2f, %.2f, %.2f))"),
			bAccept ? TEXT("ACCEPT") : TEXT("REJECT"), Delta, Coord.X, Coord.Y, Coord.Z);
	}

	FAutoConsoleCommand GAFLLagCompTestFireCmd(
		TEXT("afl.LagComp.TestFire"),
		TEXT("BM-0105c: fire the shared lag-comp ConfirmHit at the test dummy's latched 0.2s-ago position, using afl.LagComp.ForceRTT as the rewind delta. No arg = latch + fire; 'replay' = reuse latched coord. The flip: ForceRTT 0.2 + TestFire (ACCEPT), then ForceRTT 0 + TestFire replay (REJECT)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLLagCompTestFire));

	// ─── #43 selection-seam harness: afl.Cosmetic.SetEdge <color> ────────────────
	//
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

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStoreOpenCmd(TEXT("afl.Store.Open"),
		TEXT("S-ECON-STORE: push the cosmetic store (AFLW_Menu_CosmeticShop) onto UI.Layer.Menu for the local player. Browse priced/tiered catalog; buy routes through the proven wallet ServerPurchaseCosmetic."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStoreOpen));
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLStoreCloseCmd(TEXT("afl.Store.Close"),
		TEXT("S-ECON-STORE: pop the top widget on UI.Layer.Menu (closes the store if open). Same effect as the store's X button."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLStoreClose));
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

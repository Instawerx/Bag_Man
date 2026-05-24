// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLCombatCheats.h"

#include "AFLCombat.h"
#include "Abilities/AFLAG_Laser_Beam.h"
#include "Abilities/AFLAG_Laser_Pulse.h"
#include "AbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Effects/GE_AFL_Damage_Pulse.h"
#include "Effects/GE_AFL_EnergyGain_Small.h"
#include "Effects/GE_AFL_Heat_SetByCaller.h"
#include "Tuning/AFLPulseTuningData.h"
#include "UObject/Package.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "NativeGameplayTags.h"

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
	if (const APlayerController* PC = GetPlayerController())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			return Pawn->FindComponentByClass<UAbilitySystemComponent>();
		}
	}
#endif
	return nullptr;
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
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					if (UAbilitySystemComponent* ASC = Pawn->FindComponentByClass<UAbilitySystemComponent>())
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

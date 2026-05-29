// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismember.h"

#include "AbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Effects/GE_AFL_Damage_Pulse.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "HAL/IConsoleManager.h"
#include "Targeting/AFLDamageTargetSkeletal.h"

#if UE_WITH_CHEAT_MANAGER

namespace
{
	// Find the first AAFLDamageTargetSkeletal in any game world. Pattern
	// mirrors FindDummyHistory in AFLCombatCheats.cpp -- the slice expects
	// exactly one placed skeletal dummy, so the first hit wins.
	AAFLDamageTargetSkeletal* FindSkeletalDummy()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& WC : GEngine->GetWorldContexts())
		{
			UWorld* World = WC.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			for (TActorIterator<AAFLDamageTargetSkeletal> It(World); It; ++It)
			{
				return *It;
			}
		}
		return nullptr;
	}

	void HandleAFLDismemberTestKillHead(const TArray<FString>& Args)
	{
		const float Damage = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 200.0f;

		AAFLDamageTargetSkeletal* Target = FindSkeletalDummy();
		if (!Target)
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("AFL.Dismember.TestKillHead: no AAFLDamageTargetSkeletal in world -- place B_AFL_DamageTarget_Skel first"));
			return;
		}

		UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
		if (!TargetASC)
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("AFL.Dismember.TestKillHead: target has no ASC"));
			return;
		}

		// Seed Source.Damage on the (self) source ASC -- the exec calc captures
		// this with bSnapshot=true. source == target == dummy is fine because
		// the exec calc only reads the source's Damage attribute; instigator is
		// metadata, and the listener filters on Target == GetOwner() (which
		// matches when target == dummy).
		TargetASC->ApplyModToAttribute(
			UAFLAttributeSet_Combat::GetDamageAttribute(),
			EGameplayModOp::Override, Damage);

		// Context + HitResult carrying BoneName="head" -- the exec calc
		// re-fetches HitResult from the context and forwards BoneName into
		// FAFLOverkillMessage (AFLDamageExecCalc.cpp L257-267).
		FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
		Context.AddInstigator(Target, Target);

		FHitResult Hit;
		Hit.BoneName = FName(TEXT("head"));
		Hit.ImpactPoint = Target->GetActorLocation();
		Context.AddHitResult(Hit);

		FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
			UGE_AFL_Damage_Pulse::StaticClass(), /*Level=*/1.0f, Context);
		if (!SpecHandle.IsValid())
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("AFL.Dismember.TestKillHead: failed to make spec"));
			return;
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

		UE_LOG(LogAFLDismember, Display,
			TEXT("AFL.Dismember.TestKillHead: applied Damage=%.1f to %s (bone=head) -- expect Event.Damage.Overkill.AFL + dismember log"),
			Damage, *GetNameSafe(Target));
	}

	FAutoConsoleCommand GAFLDismemberTestKillHeadCmd(
		TEXT("AFL.Dismember.TestKillHead"),
		TEXT("S4-04b: lethal head-targeted hit to the first AAFLDamageTargetSkeletal, triggering Event.Damage.Overkill.AFL with BoneName=head. Usage: AFL.Dismember.TestKillHead [damage=200]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLDismemberTestKillHead));
}

#endif // UE_WITH_CHEAT_MANAGER

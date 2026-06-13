// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismember.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Effects/GE_AFL_Damage_Pulse.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "HAL/IConsoleManager.h"
#include "NativeGameplayTags.h"
#include "Targeting/AFLDamageTargetSkeletal.h"

#if UE_WITH_CHEAT_MANAGER

namespace
{
	// Leg-dismember consequence tags (declared in AFLCombatTags.ini). File-local native
	// defines for the TestLegTag cheat -- must be at namespace scope (the macro emits a
	// static FNativeGameplayTag, illegal inside a function body).
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Dismember_LeftLeg_Cheat, "State.Dismembered.LeftLeg");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Dismember_RightLeg_Cheat, "State.Dismembered.RightLeg");

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

	// S4-INC1: lethal zone-targeted hit to the first skeletal dummy, carrying an
	// arbitrary BoneName so the data-driven UAFLDismemberComponent resolves it to a
	// zone row (head -> Head row, thigh_l -> LeftLeg row, etc.). Shared by both the
	// head cheat (TestKillHead, bone fixed to "head") and the generic TestSever.
	void SeverBoneOnDummy(FName Bone, float Damage)
	{
		AAFLDamageTargetSkeletal* Target = FindSkeletalDummy();
		if (!Target)
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("AFL.Dismember: no AAFLDamageTargetSkeletal in world -- place B_AFL_DamageTarget_Skel first"));
			return;
		}

		UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
		if (!TargetASC)
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("AFL.Dismember: target has no ASC"));
			return;
		}

		// Seed Source.Damage on the (self) source ASC -- the exec calc captures this with
		// bSnapshot=true. source == target == dummy is fine: the exec calc only reads the
		// source's Damage attribute; the listener filters on Target == GetOwner() (matches
		// when target == dummy).
		TargetASC->ApplyModToAttribute(
			UAFLAttributeSet_Combat::GetDamageAttribute(),
			EGameplayModOp::Override, Damage);

		// Context + HitResult carrying the requested BoneName -- the exec calc re-fetches
		// HitResult from the context and forwards BoneName into FAFLOverkillMessage
		// (AFLDamageExecCalc.cpp L257-267) -> UAFLDismemberComponent resolves it via the ZoneSet.
		FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
		Context.AddInstigator(Target, Target);

		FHitResult Hit;
		Hit.BoneName = Bone;
		Hit.ImpactPoint = Target->GetActorLocation();
		Context.AddHitResult(Hit);

		FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
			UGE_AFL_Damage_Pulse::StaticClass(), /*Level=*/1.0f, Context);
		if (!SpecHandle.IsValid())
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("AFL.Dismember: failed to make spec"));
			return;
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

		UE_LOG(LogAFLDismember, Display,
			TEXT("AFL.Dismember: applied Damage=%.1f to %s (bone=%s) -- expect Event.Damage.Overkill.AFL + dismember log"),
			Damage, *GetNameSafe(Target), *Bone.ToString());
	}

	void HandleAFLDismemberTestKillHead(const TArray<FString>& Args)
	{
		const float Damage = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 200.0f;
		SeverBoneOnDummy(FName(TEXT("head")), Damage);
	}

	// S4-INC1: generic zone sever -- bone arg picks the zone via the data table.
	// Usage: AFL.Dismember.TestSever thigh_l [damage]   (thigh_l/thigh_r/calf_*/head/...)
	void HandleAFLDismemberTestSever(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("AFL.Dismember.TestSever: usage: AFL.Dismember.TestSever <BoneName> [damage=200] (e.g. thigh_l)"));
			return;
		}
		const FName Bone(*Args[0]);
		const float Damage = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 200.0f;
		SeverBoneOnDummy(Bone, Damage);
	}

	// S4-INC1: TOGGLE State.Dismembered.Left/RightLeg on the LOCAL PLAYER's pawn (the moving
	// hero) so the leg-speed consequence (UAFLDismemberLegPenaltyComponent) is directly
	// watchable WITHOUT severing the hero (the sever path runs on the dummy; this proves the
	// tag->penalty half on the controllable pawn -- the same isolation the dash listener used).
	// Usage: AFL.Dismember.TestLegTag [l|r]   (default l). Add once -> speed halves; again -> restores.
	void HandleAFLDismemberTestLegTag(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		const bool bRight = Args.Num() > 0 && (Args[0].Equals(TEXT("r"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("right"), ESearchCase::IgnoreCase));
		const FGameplayTag LegTag = bRight ? TAG_Dismember_RightLeg_Cheat : TAG_Dismember_LeftLeg_Cheat;

		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UAbilitySystemComponent* ASC = Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
		if (!ASC)
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("AFL.Dismember.TestLegTag: no local pawn ASC"));
			return;
		}

		// Toggle: the leg-penalty component listens NewOrRemoved, so add->apply, remove->restore.
		if (ASC->HasMatchingGameplayTag(LegTag))
		{
			ASC->RemoveLooseGameplayTag(LegTag);
			UE_LOG(LogAFLDismember, Display, TEXT("AFL.Dismember.TestLegTag: REMOVED %s from %s -> expect MaxWalkSpeed restore"),
				*LegTag.ToString(), *GetNameSafe(Pawn));
		}
		else
		{
			ASC->AddLooseGameplayTag(LegTag);
			UE_LOG(LogAFLDismember, Display, TEXT("AFL.Dismember.TestLegTag: ADDED %s to %s -> expect MaxWalkSpeed x0.5"),
				*LegTag.ToString(), *GetNameSafe(Pawn));
		}
	}

	FAutoConsoleCommand GAFLDismemberTestKillHeadCmd(
		TEXT("AFL.Dismember.TestKillHead"),
		TEXT("S4-04b: lethal head-targeted hit to the first AAFLDamageTargetSkeletal, triggering Event.Damage.Overkill.AFL with BoneName=head. Usage: AFL.Dismember.TestKillHead [damage=200]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLDismemberTestKillHead));

	FAutoConsoleCommand GAFLDismemberTestSeverCmd(
		TEXT("AFL.Dismember.TestSever"),
		TEXT("S4-INC1: lethal zone-targeted hit to the first AAFLDamageTargetSkeletal; the bone arg picks the zone via DA_AFL_DismemberZones. Usage: AFL.Dismember.TestSever <BoneName> [damage=200] (e.g. thigh_l, thigh_r)"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLDismemberTestSever));

	// World-args variant: TestLegTag toggles the leg-dismember tag on the LOCAL pawn so the
	// leg-speed penalty is watchable on the moving hero (the tag->penalty half, isolated).
	FAutoConsoleCommand GAFLDismemberTestLegTagCmd(
		TEXT("AFL.Dismember.TestLegTag"),
		TEXT("S4-INC1: TOGGLE State.Dismembered.Left/RightLeg on the local player pawn -> UAFLDismemberLegPenaltyComponent halves/restores MaxWalkSpeed. Usage: AFL.Dismember.TestLegTag [l|r]"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLDismemberTestLegTag));
}

#endif // UE_WITH_CHEAT_MANAGER

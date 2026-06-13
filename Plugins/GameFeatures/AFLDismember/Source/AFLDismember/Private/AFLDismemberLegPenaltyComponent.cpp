// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberLegPenaltyComponent.h"

#include "AFLDismember.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberLegPenaltyComponent)

// The two leg-dismember consequence tags (granted by the leg rows' ConsequenceGE in
// PHASE B). Declared in AFLCombatTags.ini next to GameplayCue.Combat.Dismember.*; native
// here to avoid the cross-plugin ini load-order race (same pattern as the dash comp's tag).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Dismembered_LeftLeg, "State.Dismembered.LeftLeg");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Dismembered_RightLeg, "State.Dismembered.RightLeg");

UAFLDismemberLegPenaltyComponent::UAFLDismemberLegPenaltyComponent()
{
	// Event-driven (tag listener), not tick-driven -- same as the proven dash comp.
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLDismemberLegPenaltyComponent::BeginPlay()
{
	Super::BeginPlay();

	// ASC resolve: DIRECT first (self-ASC'd pawns ready at BeginPlay), PawnExtension hook as the
	// FALLBACK for the possessed PLAYER (PlayerState ASC lands after pawn BeginPlay). Exact pattern
	// proven on B_Hero_BagMan by UAFLDeathComponent / UAFLDashMovementComponent.
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
		else if (ULyraPawnExtensionComponent* PawnExt = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Owner))
		{
			PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
				FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemReady));
		}
	}
}

void UAFLDismemberLegPenaltyComponent::OnAbilitySystemReady()
{
	UE_LOG(LogAFLDismember, Log, TEXT("AFL_LEGPENALTY: %s OnAbilitySystemReady -> binding leg tag listeners."),
		*GetNameSafe(GetOwner()));
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
	}
}

void UAFLDismemberLegPenaltyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bLegPenaltyActive)
	{
		RestoreLegPenalty();
	}
	UnbindFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UAFLDismemberLegPenaltyComponent::BindToAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}
	if (CachedASC.Get() == InASC && LeftLegTagHandle.IsValid())
	{
		return; // idempotent
	}
	if (CachedASC.IsValid() && CachedASC.Get() != InASC)
	{
		UnbindFromAbilitySystem(); // controller swap -> fresh PlayerState ASC
	}

	CachedASC = InASC;
	LeftLegTagHandle = InASC->RegisterGameplayTagEvent(
			TAG_State_Dismembered_LeftLeg, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLDismemberLegPenaltyComponent::HandleLegTagChanged);
	RightLegTagHandle = InASC->RegisterGameplayTagEvent(
			TAG_State_Dismembered_RightLeg, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLDismemberLegPenaltyComponent::HandleLegTagChanged);

	UE_LOG(LogAFLDismember, Log, TEXT("AFL_LEGPENALTY: %s bound leg tag listeners (ASC %s)."),
		*GetNameSafe(GetOwner()), *GetNameSafe(InASC));
}

void UAFLDismemberLegPenaltyComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (LeftLegTagHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Dismembered_LeftLeg, EGameplayTagEventType::NewOrRemoved)
				.Remove(LeftLegTagHandle);
		}
		if (RightLegTagHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Dismembered_RightLeg, EGameplayTagEventType::NewOrRemoved)
				.Remove(RightLegTagHandle);
		}
	}
	LeftLegTagHandle.Reset();
	RightLegTagHandle.Reset();
	CachedASC.Reset();
}

void UAFLDismemberLegPenaltyComponent::HandleLegTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Either leg tag changed. Drive ONE penalty off whether ANY leg is currently severed,
	// so two legs don't double-apply and one-leg-restored keeps the penalty while the other
	// holds. Read the live tag state off the ASC (NewCount is only for the tag that fired).
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		return;
	}

	const bool bAnyLegSevered =
		ASC->HasMatchingGameplayTag(TAG_State_Dismembered_LeftLeg) ||
		ASC->HasMatchingGameplayTag(TAG_State_Dismembered_RightLeg);

	if (bAnyLegSevered)
	{
		ApplyLegPenalty();
	}
	else
	{
		RestoreLegPenalty();
	}
}

UCharacterMovementComponent* UAFLDismemberLegPenaltyComponent::GetOwnerCMC() const
{
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		return Char->GetCharacterMovement();
	}
	return nullptr;
}

void UAFLDismemberLegPenaltyComponent::ApplyLegPenalty()
{
	if (bLegPenaltyActive)
	{
		return; // already scaled (the other leg, or a re-fire) -- single application
	}
	UCharacterMovementComponent* CMC = GetOwnerCMC();
	if (!CMC)
	{
		return;
	}

	// Cache at first-leg-severed -- captures the real pre-penalty speed (incl. any buffs),
	// so restore returns to that state, not a construction default. Mirrors the dash comp.
	CachedMaxWalkSpeed = CMC->MaxWalkSpeed;
	CMC->MaxWalkSpeed = CachedMaxWalkSpeed * SeveredLegSpeedScale;
	bLegPenaltyActive = true;

	UE_LOG(LogAFLDismember, Log,
		TEXT("AFL_LEGPENALTY: applied -> MaxWalkSpeed %.1f->%.1f (x%.2f)"),
		CachedMaxWalkSpeed, CMC->MaxWalkSpeed, SeveredLegSpeedScale);
}

void UAFLDismemberLegPenaltyComponent::RestoreLegPenalty()
{
	if (!bLegPenaltyActive)
	{
		return;
	}
	if (UCharacterMovementComponent* CMC = GetOwnerCMC())
	{
		CMC->MaxWalkSpeed = CachedMaxWalkSpeed;

		UE_LOG(LogAFLDismember, Log,
			TEXT("AFL_LEGPENALTY: restored -> MaxWalkSpeed->%.1f"), CMC->MaxWalkSpeed);
	}
	bLegPenaltyActive = false;
}

// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLSlideMovementComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSlideMovementComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Sliding_SlideComp, "State.Movement.Sliding");

UAFLSlideMovementComponent::UAFLSlideMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // event-driven tag listener.
}

void UAFLSlideMovementComponent::BeginPlay()
{
	Super::BeginPlay();

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

void UAFLSlideMovementComponent::OnAbilitySystemReady()
{
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
	}
}

void UAFLSlideMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bSlideActive)
	{
		RestoreSlideTuning();
	}
	UnbindFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UAFLSlideMovementComponent::BindToAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}
	if (CachedASC.Get() == InASC && SlideTagChangedHandle.IsValid())
	{
		return; // idempotent
	}
	if (CachedASC.IsValid() && CachedASC.Get() != InASC)
	{
		UnbindFromAbilitySystem();
	}

	CachedASC = InASC;
	SlideTagChangedHandle = InASC->RegisterGameplayTagEvent(
			TAG_State_Movement_Sliding_SlideComp, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLSlideMovementComponent::HandleSlideTagChanged);

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_SLIDE: %s bound slide tag listener (ASC %s)."),
		*GetNameSafe(GetOwner()), *GetNameSafe(InASC));
}

void UAFLSlideMovementComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (SlideTagChangedHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Movement_Sliding_SlideComp, EGameplayTagEventType::NewOrRemoved)
				.Remove(SlideTagChangedHandle);
		}
	}
	SlideTagChangedHandle.Reset();
	CachedASC.Reset();
}

void UAFLSlideMovementComponent::HandleSlideTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		ApplySlideTuning();
	}
	else
	{
		RestoreSlideTuning();
	}
}

UCharacterMovementComponent* UAFLSlideMovementComponent::GetOwnerCMC() const
{
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		return Char->GetCharacterMovement();
	}
	return nullptr;
}

void UAFLSlideMovementComponent::ApplySlideTuning()
{
	// Re-entrancy guard (Overdrive precedent): cache written ONLY when not already sliding.
	if (bSlideActive)
	{
		return;
	}
	UCharacterMovementComponent* CMC = GetOwnerCMC();
	if (!CMC)
	{
		return;
	}

	// Cache at slide ENTRY -> restore returns to the real pre-slide state.
	CachedGroundFriction = CMC->GroundFriction;
	CachedBrakingDeceleration = CMC->BrakingDecelerationWalking;

	CMC->GroundFriction = SlideGroundFriction;
	CMC->BrakingDecelerationWalking = SlideBrakingDeceleration;
	bSlideActive = true;

	// Duck the capsule via the CMC crouch path (lowers capsule + offsets mesh, replicated). Only if the CMC
	// permits crouch and we aren't already crouched (so UnCrouch on exit doesn't fight a real crouch).
	if (bDuckCapsuleDuringSlide)
	{
		if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
		{
			if (CMC->CanEverCrouch() && !Char->bIsCrouched)
			{
				Char->Crouch();
				bDidCrouch = true;
			}
		}
	}

	UE_LOG(LogAFLMovement, Log,
		TEXT("AFL_SLIDE: tuning applied -> friction %.2f->%.2f, braking %.0f->%.0f, ducked=%d"),
		CachedGroundFriction, CMC->GroundFriction, CachedBrakingDeceleration, CMC->BrakingDecelerationWalking, bDidCrouch ? 1 : 0);
}

void UAFLSlideMovementComponent::RestoreSlideTuning()
{
	if (!bSlideActive)
	{
		return;
	}
	if (UCharacterMovementComponent* CMC = GetOwnerCMC())
	{
		CMC->GroundFriction = CachedGroundFriction;
		CMC->BrakingDecelerationWalking = CachedBrakingDeceleration;

		UE_LOG(LogAFLMovement, Log,
			TEXT("AFL_SLIDE: tuning restored -> friction->%.2f, braking->%.0f"),
			CMC->GroundFriction, CMC->BrakingDecelerationWalking);
	}
	if (bDidCrouch)
	{
		if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
		{
			Char->UnCrouch();
		}
		bDidCrouch = false;
	}
	bSlideActive = false;
}

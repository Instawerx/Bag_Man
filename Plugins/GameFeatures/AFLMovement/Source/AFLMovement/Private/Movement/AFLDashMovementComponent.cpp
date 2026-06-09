// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLDashMovementComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDashMovementComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Dashing_DashComp, "State.Movement.Dashing");

UAFLDashMovementComponent::UAFLDashMovementComponent()
{
	// Event-driven (tag listener), not tick-driven.
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLDashMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// ASC resolve: DIRECT first (self-ASC'd pawns have it ready at BeginPlay), PawnExtension hook as the
	// FALLBACK for the possessed PLAYER (PlayerState ASC lands after pawn BeginPlay). Exact pattern proven
	// on B_Hero_BagMan by UAFLDeathComponent -- direct returns null for the player, the PawnExt hook fires
	// via LyraHeroComponent's InitializeAbilitySystem.
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

void UAFLDashMovementComponent::OnAbilitySystemReady()
{
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_DASH: %s OnAbilitySystemReady -> binding dash tag listener."),
		*GetNameSafe(GetOwner()));
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
	}
}

void UAFLDashMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bDashTuningActive)
	{
		RestoreDashTuning();
	}
	UnbindFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UAFLDashMovementComponent::BindToAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}
	if (CachedASC.Get() == InASC && DashTagChangedHandle.IsValid())
	{
		return; // idempotent
	}
	if (CachedASC.IsValid() && CachedASC.Get() != InASC)
	{
		UnbindFromAbilitySystem(); // controller swap -> fresh PlayerState ASC
	}

	CachedASC = InASC;
	DashTagChangedHandle = InASC->RegisterGameplayTagEvent(
			TAG_State_Movement_Dashing_DashComp, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLDashMovementComponent::HandleDashTagChanged);

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_DASH: %s bound dash tag listener (ASC %s)."),
		*GetNameSafe(GetOwner()), *GetNameSafe(InASC));
}

void UAFLDashMovementComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (DashTagChangedHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Movement_Dashing_DashComp, EGameplayTagEventType::NewOrRemoved)
				.Remove(DashTagChangedHandle);
		}
	}
	DashTagChangedHandle.Reset();
	CachedASC.Reset();
}

void UAFLDashMovementComponent::HandleDashTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		ApplyDashTuning();
	}
	else
	{
		RestoreDashTuning();
	}
}

UCharacterMovementComponent* UAFLDashMovementComponent::GetOwnerCMC() const
{
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		return Char->GetCharacterMovement();
	}
	return nullptr;
}

void UAFLDashMovementComponent::ApplyDashTuning()
{
	if (bDashTuningActive)
	{
		return;
	}
	UCharacterMovementComponent* CMC = GetOwnerCMC();
	if (!CMC)
	{
		return;
	}

	// Cache at dash ENTRY (§9.6) -- captures any modifications other systems applied up to this moment,
	// so restore returns to the real pre-dash state, not construction defaults.
	CachedGroundFriction = CMC->GroundFriction;
	CachedAirControl = CMC->AirControl;

	CMC->GroundFriction = DashGroundFriction;
	CMC->AirControl = DashAirControl;
	bDashTuningActive = true;

	UE_LOG(LogAFLMovement, Log,
		TEXT("AFL_DASH: tuning applied -> friction %.2f->%.2f, airControl %.2f->%.2f"),
		CachedGroundFriction, CMC->GroundFriction, CachedAirControl, CMC->AirControl);
}

void UAFLDashMovementComponent::RestoreDashTuning()
{
	if (!bDashTuningActive)
	{
		return;
	}
	if (UCharacterMovementComponent* CMC = GetOwnerCMC())
	{
		CMC->GroundFriction = CachedGroundFriction;
		CMC->AirControl = CachedAirControl;

		UE_LOG(LogAFLMovement, Log,
			TEXT("AFL_DASH: tuning restored -> friction->%.2f, airControl->%.2f"),
			CMC->GroundFriction, CMC->AirControl);
	}
	bDashTuningActive = false;
}

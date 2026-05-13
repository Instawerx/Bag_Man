// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLCharacterMovementComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCharacterMovementComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Dashing, "State.Movement.Dashing");

UAFLCharacterMovementComponent::UAFLCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UAFLCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	TryBindToAbilitySystem();
}

void UAFLCharacterMovementComponent::OnUnregister()
{
	UnbindFromAbilitySystem();
	if (bDashTuningActive)
	{
		RestoreDashTuning();
	}
	Super::OnUnregister();
}

void UAFLCharacterMovementComponent::TryBindToAbilitySystem()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
	if (!ASC)
	{
		// ASC not yet bound (PlayerState may not be replicated yet on client). The
		// Lyra init chain calls BeginPlay before ASC is guaranteed live; the dash
		// GA's first activation will route through the ASC anyway, and we'll
		// re-resolve on the first tag event. Leaving CachedASC null is fine —
		// HandleDashTagChanged guards.
		UE_LOG(LogAFLMovement, Verbose,
			TEXT("AFLCharacterMovementComponent: ASC not yet available at BeginPlay for %s; will resolve lazily"),
			*GetNameSafe(OwnerPawn));
		return;
	}

	CachedASC = ASC;
	DashTagChangedHandle = ASC->RegisterGameplayTagEvent(
			TAG_State_Movement_Dashing, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLCharacterMovementComponent::HandleDashTagChanged);

	UE_LOG(LogAFLMovement, Log,
		TEXT("AFLCharacterMovementComponent: bound dash tag listener on %s (ASC %s)"),
		*GetNameSafe(OwnerPawn), *GetNameSafe(ASC));
}

void UAFLCharacterMovementComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (DashTagChangedHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Movement_Dashing, EGameplayTagEventType::NewOrRemoved)
				.Remove(DashTagChangedHandle);
		}
	}
	DashTagChangedHandle.Reset();
	CachedASC.Reset();
}

void UAFLCharacterMovementComponent::HandleDashTagChanged(const FGameplayTag Tag, int32 NewCount)
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

void UAFLCharacterMovementComponent::ApplyDashTuning()
{
	if (bDashTuningActive)
	{
		return;
	}

	// Cache at dash ENTRY — captures any modifications applied by other systems
	// (leg-loss penalties, temporary buffs) up to the moment dash begins. This is
	// the §9.6 critical rule: restore must return to the real pre-dash state,
	// not hardcoded construction defaults.
	CachedGroundFriction = GroundFriction;
	CachedAirControl = AirControl;

	GroundFriction = DashGroundFriction;
	AirControl = DashAirControl;
	bDashTuningActive = true;

	UE_LOG(LogAFLMovement, Verbose,
		TEXT("Dash tuning applied: friction %.2f→%.2f, airControl %.2f→%.2f"),
		CachedGroundFriction, GroundFriction, CachedAirControl, AirControl);
}

void UAFLCharacterMovementComponent::RestoreDashTuning()
{
	if (!bDashTuningActive)
	{
		return;
	}

	GroundFriction = CachedGroundFriction;
	AirControl = CachedAirControl;
	bDashTuningActive = false;

	UE_LOG(LogAFLMovement, Verbose,
		TEXT("Dash tuning restored: friction→%.2f, airControl→%.2f"),
		GroundFriction, AirControl);
}

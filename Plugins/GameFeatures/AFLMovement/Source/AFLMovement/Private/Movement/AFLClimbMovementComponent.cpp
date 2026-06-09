// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLClimbMovementComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLClimbMovementComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Climbing_ClimbComp, "State.Movement.Climbing");

UAFLClimbMovementComponent::UAFLClimbMovementComponent()
{
	// Tick is OFF by default; enabled ONLY while the climb tag is active (for the surface-loss trace).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAFLClimbMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// ASC resolve: DIRECT first, PawnExtension hook FALLBACK for the possessed player. The exact pattern
	// proven on B_Hero_BagMan by UAFLDashMovementComponent / UAFLDeathComponent.
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

void UAFLClimbMovementComponent::OnAbilitySystemReady()
{
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: %s OnAbilitySystemReady -> binding climb tag listener."),
		*GetNameSafe(GetOwner()));
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
	}
}

void UAFLClimbMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bClimbStateActive)
	{
		RestoreClimbState();
	}
	UnbindFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UAFLClimbMovementComponent::BindToAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}
	if (CachedASC.Get() == InASC && ClimbTagChangedHandle.IsValid())
	{
		return; // idempotent
	}
	if (CachedASC.IsValid() && CachedASC.Get() != InASC)
	{
		UnbindFromAbilitySystem(); // controller swap -> fresh PlayerState ASC
	}

	CachedASC = InASC;
	ClimbTagChangedHandle = InASC->RegisterGameplayTagEvent(
			TAG_State_Movement_Climbing_ClimbComp, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLClimbMovementComponent::HandleClimbTagChanged);

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: %s bound climb tag listener (ASC %s)."),
		*GetNameSafe(GetOwner()), *GetNameSafe(InASC));
}

void UAFLClimbMovementComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (ClimbTagChangedHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Movement_Climbing_ClimbComp, EGameplayTagEventType::NewOrRemoved)
				.Remove(ClimbTagChangedHandle);
		}
	}
	ClimbTagChangedHandle.Reset();
	CachedASC.Reset();
}

void UAFLClimbMovementComponent::HandleClimbTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		ApplyClimbState();
	}
	else
	{
		RestoreClimbState();
	}
}

UCharacterMovementComponent* UAFLClimbMovementComponent::GetOwnerCMC() const
{
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		return Char->GetCharacterMovement();
	}
	return nullptr;
}

void UAFLClimbMovementComponent::ApplyClimbState()
{
	if (bClimbStateActive)
	{
		return;
	}
	UCharacterMovementComponent* CMC = GetOwnerCMC();
	if (!CMC)
	{
		return;
	}

	// Cache at climb ENTRY (mirrors dash) so restore returns to the real pre-climb state.
	CachedGravityScale = CMC->GravityScale;
	CachedMovementMode = CMC->MovementMode;

	CMC->GravityScale = 0.0f;
	CMC->SetMovementMode(MOVE_Flying); // hold on the wall plane; the ability's root motion drives the climb up.
	bClimbStateActive = true;

	// Enable the surface-loss trace tick only while climbing.
	SetComponentTickEnabled(true);

	UE_LOG(LogAFLMovement, Log,
		TEXT("AFL_CLIMB: state applied -> gravity %.2f->0.00, mode %d->Flying."),
		CachedGravityScale, (int32)CachedMovementMode.GetValue());
}

void UAFLClimbMovementComponent::RestoreClimbState()
{
	if (!bClimbStateActive)
	{
		return;
	}
	SetComponentTickEnabled(false);

	if (UCharacterMovementComponent* CMC = GetOwnerCMC())
	{
		CMC->GravityScale = CachedGravityScale;
		// Restore to Walking explicitly (we left from Walking; SetMovementMode resolves Falling if airborne).
		CMC->SetMovementMode(CachedMovementMode);

		UE_LOG(LogAFLMovement, Log,
			TEXT("AFL_CLIMB: state restored -> gravity->%.2f, mode->%d."),
			CMC->GravityScale, (int32)CachedMovementMode.GetValue());
	}
	bClimbStateActive = false;
}

void UAFLClimbMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bClimbStateActive)
	{
		return;
	}

	// Surface-loss check: forward trace from the pawn origin; if the wall is gone, signal the ability to exit.
	const AActor* Owner = GetOwner();
	const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!Owner || !World)
	{
		return;
	}

	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + Owner->GetActorForwardVector() * SurfaceTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLClimbSurfaceTrace), /*bTraceComplex=*/false, Owner);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	if (!bHit)
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: surface lost (forward trace %.0fcm hit nothing)."), SurfaceTraceDistance);
		// Stop ticking immediately so we broadcast exactly once; the ability's handler ends the ability,
		// which removes the GE -> tag clears -> RestoreClimbState runs.
		SetComponentTickEnabled(false);
		OnClimbSurfaceLost.Broadcast();
	}
}

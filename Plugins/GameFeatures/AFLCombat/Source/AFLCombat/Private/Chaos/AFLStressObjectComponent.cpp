// Copyright C12 AI Gaming. All Rights Reserved.

#include "Chaos/AFLStressObjectComponent.h"

#include "AFLCombat.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLStressObjectComponent)

static TAutoConsoleVariable<float> CVarAFLChaosInstabilitySeconds(
	TEXT("afl.Chaos.InstabilitySeconds"),
	45.0f,
	TEXT("Seconds the stress object can be held before it repositions (forces a re-grab). S10 AFL-0804."));


UAFLStressObjectComponent::UAFLStressObjectComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // the instability clock.
}

void UAFLStressObjectComponent::BeginPlay()
{
	Super::BeginPlay();
	// Server-only: the reposition is an authority teleport (replicates via the grabbable's RepMovement).
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
	}
}

bool UAFLStressObjectComponent::IsOwnerHeld() const
{
	// Module-decoupled read: the grabbable lives in AFLMovement (no AFLCombat dep). Find the component
	// whose class is named "AFLGrabbableComponent" and read its replicated bHeld UPROPERTY reflectively
	// (the same value IsHeld() returns) -- so the stress component carries no cross-GameFeature dep.
	const AActor* Owner = GetOwner();
	if (!Owner) { return false; }
	for (UActorComponent* Comp : Owner->GetComponents())
	{
		if (Comp && Comp->GetClass()->GetName().Contains(TEXT("AFLGrabbableComponent")))
		{
			if (const FBoolProperty* HeldProp = CastField<FBoolProperty>(Comp->GetClass()->FindPropertyByName(TEXT("bHeld"))))
			{
				return HeldProp->GetPropertyValue_InContainer(Comp);
			}
		}
	}
	return false;
}

void UAFLStressObjectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const bool bHeld = IsOwnerHeld();
	if (!bHeld)
	{
		HeldSeconds = 0.0f; // reset on release -- the timer is for CONTINUOUS holding.
		bWasHeld = false;
		return;
	}
	bWasHeld = true;
	HeldSeconds += DeltaTime;

	const float Limit = FMath::Max(1.0f, CVarAFLChaosInstabilitySeconds.GetValueOnGameThread());
	if (HeldSeconds >= Limit)
	{
		// Reposition: shove the object RepositionDistance away in a deterministic direction (the
		// owner's +X), forcing a re-grab. The grabbable's release/re-grab path handles the rest.
		// (A nav-point pick is a later polish; a flat offset is the cycle-1 reposition.)
		const FVector NewLoc = Owner->GetActorLocation() + Owner->GetActorForwardVector() * RepositionDistance + FVector(0, 0, 50.0f);
		Owner->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
		HeldSeconds = 0.0f;
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_CHAOS: stress object %s INSTABILITY -- repositioned (held > %.0fs)."),
			*GetNameSafe(Owner), Limit);
	}
}

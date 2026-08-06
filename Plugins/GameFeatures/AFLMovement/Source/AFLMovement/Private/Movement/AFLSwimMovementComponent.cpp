// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLSwimMovementComponent.h"

#include "AFLMovement.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSwimMovementComponent)

namespace
{
	/**
	 * Readable movement-mode name, emitted beside the raw value so a reader never has to map the enum.
	 *
	 * ⚠ THE ORDINALS ARE A TRAP. EMovementMode has NO explicit values -- it is purely ordinal:
	 *     None=0  Walking=1  NavWalking=2  Falling=3  Swimming=4  Flying=5  Custom=6
	 * so MOVE_Swimming IS 4 AND MOVE_Falling IS 3. Anyone grepping for "-> 3" expecting swim is watching
	 * FALLING, which fires on every jump, every step off a ledge, and THE FALL INTO WATER -- so it reads as
	 * success at exactly the moment it means the opposite. Logging the name is what stops that recurring.
	 */
	const TCHAR* AFLSwimModeName(EMovementMode Mode)
	{
		switch (Mode)
		{
		case MOVE_None:       return TEXT("None");
		case MOVE_Walking:    return TEXT("Walking");
		case MOVE_NavWalking: return TEXT("NavWalking");
		case MOVE_Falling:    return TEXT("Falling");
		case MOVE_Swimming:   return TEXT("Swimming");
		case MOVE_Flying:     return TEXT("Flying");
		case MOVE_Custom:     return TEXT("Custom");
		default:              return TEXT("Unknown");
		}
	}
}

UAFLSwimMovementComponent::UAFLSwimMovementComponent()
{
	// No tick. The tuning is applied once at BeginPlay and the instrument runs off a delegate, so there is
	// nothing to poll. (Sprint keeps a timer only for its diagnostic ramp sampling; swim has no ramp.)
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLSwimMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// LIFECYCLE: BeginPlay is the right point, and it is EARLIER than what sprint needs.
	//
	// Sprint binds the owner's ASC, which for a possessed player does not exist until Lyra's PawnExtension
	// init-state lifecycle finishes -- well after pawn BeginPlay -- so sprint carries a deferred callback.
	// Swim tuning touches NO ASC and NO tag: it needs only the CMC, which is a construction-time subobject
	// of ACharacter and is therefore already present here. No deferral is needed, so none is copied.
	ApplySwimTuning();

	// Bind the instrument to the pawn's own delegate rather than overriding the CMC. This is the whole point
	// of this component: a hook on the class that ACTUALLY EXISTS on the pawn.
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		// AddUniqueDynamic, not AddDynamic -- X3: AddDynamic is not idempotent, and a double-bind would
		// double-log every transition and make an intermittent read look like a repeating one.
		Char->MovementModeChangedDelegate.AddUniqueDynamic(this, &UAFLSwimMovementComponent::HandleMovementModeChanged);
	}
	else
	{
		UE_LOG(LogAFLMovement, Warning,
			TEXT("AFLSwimMovementComponent: owner %s is not an ACharacter -- swim tuning and the movement-mode instrument are both INERT here."),
			*GetNameSafe(GetOwner()));
	}
}

void UAFLSwimMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		Char->MovementModeChangedDelegate.RemoveDynamic(this, &UAFLSwimMovementComponent::HandleMovementModeChanged);
	}

	// No tuning restore. Unlike sprint there is no cached pre-state to return to: these are static values
	// applied once, and the CMC dies with the pawn.

	Super::EndPlay(EndPlayReason);
}

UCharacterMovementComponent* UAFLSwimMovementComponent::GetOwnerCMC() const
{
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	return Char ? Char->GetCharacterMovement() : nullptr;
}

void UAFLSwimMovementComponent::ApplySwimTuning()
{
	UCharacterMovementComponent* CMC = GetOwnerCMC();
	if (!CMC)
	{
		UE_LOG(LogAFLMovement, Warning,
			TEXT("AFLSwimMovementComponent: no CharacterMovementComponent on %s -- swim tuning NOT applied."),
			*GetNameSafe(GetOwner()));
		return;
	}

	const float PrevSwimSpeed = CMC->MaxSwimSpeed;
	const float PrevBuoyancy  = CMC->Buoyancy;

	CMC->MaxSwimSpeed = MaxSwimSpeed;
	CMC->Buoyancy     = Buoyancy;
	bTuningApplied    = true;

	// Logged at Log level, not Verbose. This line is the proof the tuning reached a REAL CMC -- the exact
	// evidence missing when the values lived on a class nothing constructed. It names the CMC's concrete
	// class so a future reader can see at a glance which component was actually written to.
	UE_LOG(LogAFLMovement, Log,
		TEXT("AFL_SWIMTUNE: owner=%s cmc=%s MaxSwimSpeed %.1f->%.1f Buoyancy %.2f->%.2f"),
		*GetNameSafe(GetOwner()), *GetNameSafe(CMC->GetClass()),
		PrevSwimSpeed, CMC->MaxSwimSpeed, PrevBuoyancy, CMC->Buoyancy);
}

void UAFLSwimMovementComponent::HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
#if !UE_BUILD_SHIPPING
	// Guarded to match the project convention (UAFLBattleRoyaleComponent uses the same #if for its
	// belief-state dump). Diagnostics cost in shipping; this stays in Development and Test, where it is read.
	const UCharacterMovementComponent* CMC = Character ? Character->GetCharacterMovement() : nullptr;
	if (!CMC)
	{
		return;
	}

	const APhysicsVolume* Volume = CMC->GetPhysicsVolume();

	UE_LOG(LogAFLMovement, Log,
		TEXT("AFL_MOVEMODE: owner=%s %s(%d) -> %s(%d) inWater=%d volume=%s MaxSwimSpeed=%.1f Buoyancy=%.2f vel=%.1f"),
		*GetNameSafe(Character),
		AFLSwimModeName(PrevMovementMode),          (int32)PrevMovementMode,
		AFLSwimModeName(CMC->MovementMode.GetValue()), (int32)CMC->MovementMode.GetValue(),
		(Volume && Volume->bWaterVolume) ? 1 : 0,
		*GetNameSafe(Volume),
		CMC->MaxSwimSpeed,
		CMC->Buoyancy,
		CMC->Velocity.Size());
#endif // !UE_BUILD_SHIPPING
}

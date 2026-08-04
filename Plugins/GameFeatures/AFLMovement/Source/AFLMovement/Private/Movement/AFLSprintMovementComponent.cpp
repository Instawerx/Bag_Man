// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLSprintMovementComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSprintMovementComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Sprinting_SprintComp, "State.Movement.Sprinting");

UAFLSprintMovementComponent::UAFLSprintMovementComponent()
{
	// Event-driven (tag listener), not tick-driven.
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLSprintMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// ASC resolve: DIRECT first (self-ASC'd pawns have it ready at BeginPlay), PawnExtension hook as the
	// FALLBACK for the possessed PLAYER (PlayerState ASC lands after pawn BeginPlay). Exact pattern proven
	// on B_Hero_BagMan by UAFLDashMovementComponent / UAFLDeathComponent.
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

void UAFLSprintMovementComponent::OnAbilitySystemReady()
{
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_SPRINT: %s OnAbilitySystemReady -> binding sprint tag listener."),
		*GetNameSafe(GetOwner()));
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
	}
}

void UAFLSprintMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bSprintSwapped)
	{
		RestoreSprintTuning();
	}
	UnbindFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UAFLSprintMovementComponent::BindToAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}
	if (CachedASC.Get() == InASC && SprintTagChangedHandle.IsValid())
	{
		return; // idempotent
	}
	if (CachedASC.IsValid() && CachedASC.Get() != InASC)
	{
		UnbindFromAbilitySystem(); // controller swap -> fresh PlayerState ASC
	}

	CachedASC = InASC;
	SprintTagChangedHandle = InASC->RegisterGameplayTagEvent(
			TAG_State_Movement_Sprinting_SprintComp, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLSprintMovementComponent::HandleSprintTagChanged);

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_SPRINT: %s bound sprint tag listener (ASC %s)."),
		*GetNameSafe(GetOwner()), *GetNameSafe(InASC));
}

void UAFLSprintMovementComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (SprintTagChangedHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Movement_Sprinting_SprintComp, EGameplayTagEventType::NewOrRemoved)
				.Remove(SprintTagChangedHandle);
		}
	}
	SprintTagChangedHandle.Reset();
	CachedASC.Reset();
}

void UAFLSprintMovementComponent::HandleSprintTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		ApplySprintTuning();
	}
	else
	{
		RestoreSprintTuning();
	}
}

UCharacterMovementComponent* UAFLSprintMovementComponent::GetOwnerCMC() const
{
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		return Char->GetCharacterMovement();
	}
	return nullptr;
}

void UAFLSprintMovementComponent::ApplySprintTuning()
{
	// Re-entrancy guard (Overdrive precedent): cache is written ONLY when not already swapped, so a duplicate
	// rise event can never bake the boosted value in as the restore target.
	if (bSprintSwapped)
	{
		return;
	}
	UCharacterMovementComponent* CMC = GetOwnerCMC();
	if (!CMC)
	{
		return;
	}

	// Cache at sprint ENTRY -- captures modifications other systems applied up to this moment (e.g. a live
	// Overdrive buff), so restore returns to the real pre-sprint state, not construction defaults.
	CachedMaxWalkSpeed = CMC->MaxWalkSpeed;
	CachedMaxAcceleration = CMC->MaxAcceleration;

	CMC->MaxWalkSpeed = CachedMaxWalkSpeed * SprintSpeedMultiplier;
	CMC->MaxAcceleration = CachedMaxAcceleration * SprintAccelMultiplier;
	bSprintSwapped = true;
	SprintStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// PAWN NAME IS LOAD-BEARING, not decoration. Without it these lines are unattributable, and on a 16-bot
	// match the player's two presses are lost among 55 bot sprints -- which produced a confident wrong
	// diagnosis (a bot's 1.25s lease expiry read as the player's 5s hold ending early).
	UE_LOG(LogAFLMovement, Log,
		TEXT("AFL_SPRINT: %s tuning applied -> speed %.0f->%.0f, accel %.0f->%.0f"),
		*GetNameSafe(GetOwner()), CachedMaxWalkSpeed, CMC->MaxWalkSpeed, CachedMaxAcceleration, CMC->MaxAcceleration);

	// 0.05s, not 0.25s. The question is the SHAPE of the 700->980 ramp, and at 0.25s a ramp that completes in
	// ~0.3s yields two samples -- enough to say it happened, not enough to say how. 20Hz gives ~6 points across
	// the same window, which is what an acceleration proposal has to be argued against.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SprintDiagTimer,
			FTimerDelegate::CreateWeakLambda(this, [this] { TickSprintDiag(); }),
			0.05f, /*bLoop*/ true);
	}
}

void UAFLSprintMovementComponent::TickSprintDiag()
{
	const UCharacterMovementComponent* CMC = GetOwnerCMC();
	const AActor* Owner = GetOwner();
	const APawn* Pawn = Cast<APawn>(Owner);
	if (!CMC || !Pawn)
	{
		return;
	}

	// TOLD vs DOING. MaxWalkSpeed is what we set; Velocity is what the character actually achieves. Role tells
	// us WHICH instance is speaking -- if the player pawn has a server and a client instance and only one
	// swapped, correction pulls the player back to walking speed while both apply-logs read correct.
	//
	// t= is time SINCE THE SWAP, not wall clock. The onset question is "how long from press to top speed", and
	// answering it off log timestamps means parsing them and assuming the sampler and the swap started together.
	// Carrying the offset in the payload makes the ramp readable straight off the line.
	const float SinceOnset = GetWorld() ? (GetWorld()->GetTimeSeconds() - SprintStartTime) : 0.0f;
	UE_LOG(LogAFLMovement, Log,
		TEXT("AFL_SPRINTDIAG: %-26s t=%.2f auth=%d localCtl=%d role=%d | MaxWalkSpeed=%.0f MaxAccel=%.0f Velocity2D=%.0f"),
		*GetNameSafe(Owner),
		SinceOnset,
		Pawn->HasAuthority() ? 1 : 0,
		Pawn->IsLocallyControlled() ? 1 : 0,
		static_cast<int32>(Pawn->GetLocalRole()),
		CMC->MaxWalkSpeed,
		CMC->GetMaxAcceleration(),
		CMC->Velocity.Size2D());
}

void UAFLSprintMovementComponent::RestoreSprintTuning()
{
	if (!bSprintSwapped)
	{
		return;
	}
	if (UCharacterMovementComponent* CMC = GetOwnerCMC())
	{
		CMC->MaxWalkSpeed = CachedMaxWalkSpeed;
		CMC->MaxAcceleration = CachedMaxAcceleration;

		UE_LOG(LogAFLMovement, Log,
			TEXT("AFL_SPRINT: %s tuning restored -> speed->%.0f, accel->%.0f"),
			*GetNameSafe(GetOwner()), CMC->MaxWalkSpeed, CMC->MaxAcceleration);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SprintDiagTimer);
	}
	bSprintSwapped = false;
}

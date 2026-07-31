// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLWallRunMovementComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLWallRunMovementComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_WallRunning_WRComp, "State.Movement.WallRunning");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Movement_WallRun_Detected, "Event.Movement.WallRun.Detected");

UAFLWallRunMovementComponent::UAFLWallRunMovementComponent()
{
	// Ticks (detection while airborne + physics while wall-running); the tick body early-outs when grounded.
	PrimaryComponentTick.bCanEverTick = true;
}

void UAFLWallRunMovementComponent::BeginPlay()
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

void UAFLWallRunMovementComponent::OnAbilitySystemReady()
{
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
	}
}

void UAFLWallRunMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bWallRunActive)
	{
		ExitWallRunState();
	}
	UnbindFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UAFLWallRunMovementComponent::BindToAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}
	if (CachedASC.Get() == InASC && WallRunTagHandle.IsValid())
	{
		return;
	}
	if (CachedASC.IsValid() && CachedASC.Get() != InASC)
	{
		UnbindFromAbilitySystem();
	}
	CachedASC = InASC;
	WallRunTagHandle = InASC->RegisterGameplayTagEvent(
			TAG_State_Movement_WallRunning_WRComp, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLWallRunMovementComponent::HandleWallRunTagChanged);
}

void UAFLWallRunMovementComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (WallRunTagHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Movement_WallRunning_WRComp, EGameplayTagEventType::NewOrRemoved)
				.Remove(WallRunTagHandle);
		}
	}
	WallRunTagHandle.Reset();
	CachedASC.Reset();
}

void UAFLWallRunMovementComponent::HandleWallRunTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		EnterWallRunState();
	}
	else
	{
		ExitWallRunState();
	}
}

UCharacterMovementComponent* UAFLWallRunMovementComponent::GetOwnerCMC() const
{
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		return Char->GetCharacterMovement();
	}
	return nullptr;
}

void UAFLWallRunMovementComponent::EnterWallRunState()
{
	if (bWallRunActive)
	{
		return;
	}
	UCharacterMovementComponent* CMC = GetOwnerCMC();
	if (!CMC)
	{
		return;
	}
	CachedGravityScale = CMC->GravityScale;
	CMC->GravityScale = 0.0f;
	CMC->SetMovementMode(MOVE_Flying);
	bWallRunActive = true;
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_WALLRUN: enter state (gravity 0 / flying)."));
}

void UAFLWallRunMovementComponent::ExitWallRunState()
{
	if (!bWallRunActive)
	{
		return;
	}
	if (UCharacterMovementComponent* CMC = GetOwnerCMC())
	{
		CMC->GravityScale = (CachedGravityScale >= 0.0f) ? CachedGravityScale : 1.0f;
		CMC->SetMovementMode(MOVE_Falling); // exits into the air
	}
	bWallRunActive = false;
	CurrentWallNormal = FVector::ZeroVector;
	ReattachTimer = ReattachCooldown;
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_WALLRUN: exit state (restored gravity, falling)."));
}

void UAFLWallRunMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ReattachTimer > 0.0f)
	{
		ReattachTimer -= DeltaTime;
	}

	if (bWallRunActive)
	{
		TickWallRunPhysics(DeltaTime);
	}
	else
	{
		TickDetection(DeltaTime);
	}
}

void UAFLWallRunMovementComponent::TickDetection(float DeltaTime)
{
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* CMC = GetOwnerCMC();
	if (!Char || !CMC || !CMC->IsFalling() || ReattachTimer > 0.0f)
	{
		return; // only detect while airborne, and not during the post-walljump cooldown
	}
	if (CMC->Velocity.Size2D() < MinForwardSpeed)
	{
		return;
	}
	DetectAccum += DeltaTime;
	if (DetectAccum < DetectionInterval)
	{
		return;
	}
	DetectAccum = 0.0f;

	FHitResult HitL, HitR;
	const bool bL = SideTrace(-Char->GetActorRightVector(), DetectionSideDistance, HitL);
	const bool bR = SideTrace(Char->GetActorRightVector(), DetectionSideDistance, HitR);
	if (bL || bR)
	{
		// Contextual trigger: the ability activates off this event (no keypress). It re-confirms the wall via
		// this component's per-tick trace once State.Movement.WallRunning is applied.
		if (AActor* Owner = GetOwner())
		{
			FGameplayEventData Payload;
			Payload.Instigator = Owner;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, TAG_Event_Movement_WallRun_Detected, Payload);
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_WALLRUN: wall detected (%s) -> event."), bL ? TEXT("left") : TEXT("right"));
		}
	}
}

void UAFLWallRunMovementComponent::TickWallRunPhysics(float DeltaTime)
{
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* CMC = GetOwnerCMC();
	if (!Char || !CMC)
	{
		return;
	}

	// Which side is the wall? Trace both; take whichever hits.
	FHitResult HitL, HitR, Hit;
	const bool bL = SideTrace(-Char->GetActorRightVector(), WallLossTraceDistance, HitL);
	const bool bR = SideTrace(Char->GetActorRightVector(), WallLossTraceDistance, HitR);
	if (bL)
	{
		Hit = HitL;
	}
	else if (bR)
	{
		Hit = HitR;
	}
	else
	{
		// Wall gone -> the ability listens to this and exits (which clears the tag -> ExitWallRunState).
		OnWallSurfaceLost.Broadcast();
		return;
	}

	CurrentWallNormal = Hit.ImpactNormal;

	// Tangent along the wall (horizontal), oriented along the pawn's current facing.
	FVector Tangent = FVector::CrossProduct(CurrentWallNormal, FVector::UpVector).GetSafeNormal();
	if (FVector::DotProduct(Tangent, Char->GetActorForwardVector()) < 0.0f)
	{
		Tangent = -Tangent;
	}

	// Drive velocity: forward along the wall + stick toward it + optional downward drift.
	FVector Vel = Tangent * WallRunSpeed - CurrentWallNormal * WallStickForce;
	Vel.Z -= WallRunDownDrift;
	CMC->Velocity = Vel;
}

bool UAFLWallRunMovementComponent::SideTrace(const FVector& Dir, float Distance, FHitResult& OutHit) const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!Owner || !World)
	{
		return false;
	}
	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + Dir.GetSafeNormal() * Distance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLWallRunSideTrace), /*bTraceComplex*/ false, Owner);
	return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
}

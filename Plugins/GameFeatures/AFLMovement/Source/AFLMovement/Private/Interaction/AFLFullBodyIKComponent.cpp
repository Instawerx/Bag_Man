// Copyright C12 AI Gaming. All Rights Reserved.

#include "Interaction/AFLFullBodyIKComponent.h"

#include "AFLMovement.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UAFLFullBodyIKComponent::UAFLFullBodyIKComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Default to the Pro FBIK post-process ABP (the generated anim-instance class).
	PostProcessABP = TSoftClassPtr<UAnimInstance>(
		FSoftObjectPath(TEXT("/Game/BagMan/ProMod/ABP_ProMod_FBIK_PP.ABP_ProMod_FBIK_PP_C")));
}

void UAFLFullBodyIKComponent::BeginPlay()
{
	Super::BeginPlay();

	// Resolve the owning character's mesh (the WeaponIK component uses the same Cast<ACharacter>->GetMesh()).
	USkeletalMeshComponent* Mesh = nullptr;
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Mesh = Character->GetMesh();
	}
	if (!Mesh)
	{
		UE_LOG(LogAFLMovement, Warning, TEXT("AFL_FBIK: %s has no character mesh -- post-process not applied."),
			*GetNameSafe(GetOwner()));
		return;
	}

	TSubclassOf<UAnimInstance> ABPClass = PostProcessABP.LoadSynchronous();
	if (!ABPClass)
	{
		UE_LOG(LogAFLMovement, Warning, TEXT("AFL_FBIK: PostProcessABP failed to load (%s) -- no FBIK on %s."),
			*PostProcessABP.ToString(), *GetNameSafe(GetOwner()));
		return;
	}

	// Per-component override (transient/runtime): scoped to THIS Pro pawn, shared assets untouched.
	// ReinitAnimInstances=true because we are past construction (the game is running).
	Mesh->SetOverridePostProcessAnimBP(ABPClass, /*ReinitAnimInstances=*/true);
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_FBIK: applied post-process ABP '%s' to %s (Pro full-body IK live)."),
		*ABPClass->GetName(), *GetNameSafe(GetOwner()));
}

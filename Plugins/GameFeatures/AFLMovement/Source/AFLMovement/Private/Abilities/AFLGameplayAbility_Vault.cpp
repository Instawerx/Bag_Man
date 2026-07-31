// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Vault.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "MotionWarpingComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Vault)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Vault_State_Match_Warmup, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Vault_State_Match_Ended, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Vault_State_Extracting, "State.Extracting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Vault_State_Movement_Climbing, "State.Movement.Climbing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Vault_State_Carrying, "State.Carrying");

UAFLGameplayAbility_Vault::UAFLGameplayAbility_Vault(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(TAG_Vault_State_Match_Warmup);
	ActivationBlockedTags.AddTag(TAG_Vault_State_Match_Ended);
	ActivationBlockedTags.AddTag(TAG_Vault_State_Extracting);
	ActivationBlockedTags.AddTag(TAG_Vault_State_Movement_Climbing); // can't vault while climbing
	ActivationBlockedTags.AddTag(TAG_Vault_State_Carrying);          // can't vault while carrying
}

void UAFLGameplayAbility_Vault::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bExiting = false;
	bWarpApplied = false;
	MontageTask = nullptr;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Probe for a vaultable obstacle. No obstacle in front -> not a vault; cancel (a no-op press).
	FVector TopEdge, Landing;
	FRotator Facing;
	if (!DetectVault(TopEdge, Landing, Facing))
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_VAULT: no vaultable obstacle in front -> cancel."));
		CancelAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateCancelAbility*/ true);
		return;
	}

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_VAULT: activate by %s (top=%s, land=%s)."),
		*GetNameSafe(Character), *TopEdge.ToCompactString(), *Landing.ToCompactString());

	// Motion Warping -> skew the fixed-distance vault montage onto this obstacle's top edge + far-side landing.
	if (UMotionWarpingComponent* MotionWarping = Character->FindComponentByClass<UMotionWarpingComponent>())
	{
		MotionWarping->bSearchForWindowsInAnimsWithinMontages = true; // warp windows live on sub-anims (idempotent)
		MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(VaultTopWarpTargetName, TopEdge, Facing);
		MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(VaultLandWarpTargetName, Landing, Facing);
		bWarpApplied = true;
	}
	else
	{
		UE_LOG(LogAFLMovement, Warning, TEXT("AFL_VAULT: no MotionWarpingComponent on hero -> vault uses raw montage distances."));
	}

	// Apply the vault-active GE -> grants State.Movement.Vaulting (blocks conflicting moves; drives AI/anim).
	if (VaultActiveEffectClass)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(VaultActiveEffectClass, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}

	// Play the root-motion vault montage. Root motion (+ the warp targets) carries the character over.
	if (VaultMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, VaultMontage, /*Rate*/ 1.0f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UAFLGameplayAbility_Vault::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UAFLGameplayAbility_Vault::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UAFLGameplayAbility_Vault::OnMontageInterruptedOrCancelled);
			MontageTask->OnCancelled.AddDynamic(this, &UAFLGameplayAbility_Vault::OnMontageInterruptedOrCancelled);
			MontageTask->ReadyForActivation();
		}
	}
	else
	{
		// No montage authored yet -> warp targets set but nothing to translate the character. Exit so we don't hang.
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_VAULT: no VaultMontage set -> nothing to play (placeholder)."));
		ExitVault(TEXT("no-montage"), /*bCancelled*/ true);
	}
}

bool UAFLGameplayAbility_Vault::DetectVault(FVector& OutTopEdge, FVector& OutLanding, FRotator& OutFacing) const
{
	const ACharacter* Character = GetCurrentActorInfo() ? Cast<ACharacter>(GetCurrentActorInfo()->AvatarActor.Get()) : nullptr;
	const UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		return false;
	}

	const float HalfHeight = Character->GetSimpleCollisionHalfHeight();
	const FVector Loc = Character->GetActorLocation();
	const float FeetZ = Loc.Z - HalfHeight;

	FVector Forward = Character->GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLVaultTrace), /*bTraceComplex*/ false, Character);

	// 1. Forward trace at ~mid-shin/knee height so a waist-high obstacle is hit but the ground is not.
	const FVector FwdStart = FVector(Loc.X, Loc.Y, FeetZ + 50.0f);
	const FVector FwdEnd = FwdStart + Forward * ForwardTraceDistance;
	FHitResult FwdHit;
	if (!World->LineTraceSingleByChannel(FwdHit, FwdStart, FwdEnd, ECC_Visibility, Params))
	{
		return false; // nothing in front
	}
	const FVector FrontPoint = FwdHit.ImpactPoint;

	// Facing = into the obstacle (opposite its front-face normal), yaw only.
	FVector FaceDir = -FwdHit.ImpactNormal;
	FaceDir.Z = 0.0f;
	if (!FaceDir.Normalize())
	{
		FaceDir = Forward;
	}
	OutFacing = FaceDir.Rotation();

	// 2. Top-edge trace: straight down from above the obstacle, just past its front face, to find the top surface.
	const FVector ProbeXY = FrontPoint + Forward * 20.0f;
	const FVector TopStart = FVector(ProbeXY.X, ProbeXY.Y, FeetZ + MaxObstacleHeight + 20.0f);
	const FVector TopEnd = FVector(ProbeXY.X, ProbeXY.Y, FeetZ - 10.0f);
	FHitResult TopHit;
	if (!World->LineTraceSingleByChannel(TopHit, TopStart, TopEnd, ECC_Visibility, Params))
	{
		return false; // no top within reach (wall too tall / no ledge to vault)
	}
	const float TopZ = TopHit.ImpactPoint.Z;
	const float Height = TopZ - FeetZ;
	if (Height < MinObstacleHeight || Height > MaxObstacleHeight)
	{
		return false; // too low (just step over) or too tall (that's a climb, not a vault)
	}

	// Hand-plant point: the front-top edge (front-face XY at the top surface Z).
	OutTopEdge = FVector(FrontPoint.X, FrontPoint.Y, TopZ);

	// 3. Landing trace: beyond the obstacle, trace down for a place to land.
	const FVector LandXY = ProbeXY + Forward * LandingProbeDistance;
	const FVector LandStart = FVector(LandXY.X, LandXY.Y, TopZ + 50.0f);
	const FVector LandEnd = FVector(LandXY.X, LandXY.Y, FeetZ - 120.0f);
	FHitResult LandHit;
	if (World->LineTraceSingleByChannel(LandHit, LandStart, LandEnd, ECC_Visibility, Params))
	{
		OutLanding = LandHit.ImpactPoint + FVector::UpVector * HalfHeight; // stand on the landing surface
	}
	else
	{
		// No landing surface found within the drop probe -> land at the same feet level beyond (open ground).
		OutLanding = FVector(LandXY.X, LandXY.Y, FeetZ + HalfHeight);
	}

	return true;
}

void UAFLGameplayAbility_Vault::OnMontageCompleted()
{
	ExitVault(TEXT("complete"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_Vault::OnMontageInterruptedOrCancelled()
{
	ExitVault(TEXT("interrupted"), /*bCancelled*/ true);
}

void UAFLGameplayAbility_Vault::ExitVault(const TCHAR* Reason, bool bCancelled)
{
	if (bExiting)
	{
		return; // first exit wins
	}
	bExiting = true;
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_VAULT: exit (reason=%s)."), Reason);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility*/ true, bCancelled);
}

void UAFLGameplayAbility_Vault::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Clear the warp targets this ability added (mirror Climb/Slide).
	if (bWarpApplied)
	{
		if (const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
		{
			if (UMotionWarpingComponent* MotionWarping = Character->FindComponentByClass<UMotionWarpingComponent>())
			{
				MotionWarping->RemoveAllWarpTargets();
			}
		}
		bWarpApplied = false;
	}

	// Remove the vault-active GE so State.Movement.Vaulting clears on every exit path.
	if (VaultActiveEffectClass && ActorInfo)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(VaultActiveEffectClass, ASC);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

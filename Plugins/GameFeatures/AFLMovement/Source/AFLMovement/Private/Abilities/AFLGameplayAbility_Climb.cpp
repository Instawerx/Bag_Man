// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Climb.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "MotionWarpingComponent.h"
#include "TimerManager.h"
#include "GameplayEffect.h"
#include "Movement/AFLClimbMovementComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Climb)

// Native ability tag (mirrors the dash GA's native-tag pattern; removes cross-plugin ini load-order
// fragility). The ability's own AbilityTags should include Ability.Movement.Climb (set on the BP child
// or here if desired); the activation-block tags mirror dash.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Movement_Climb, "Ability.Movement.Climb");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Climb_State_Match_Warmup, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Climb_State_Match_Ended, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Climb_State_Extracting, "State.Extracting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Climb_State_Carrying, "State.Carrying");

UAFLGameplayAbility_Climb::UAFLGameplayAbility_Climb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// WhileInputActive owns the held lifecycle (the working-sibling pattern from UAFLAG_BeamChannel_v2):
	// Lyra's ProcessAbilityInput activates the ability ONCE while the input is held -- it does NOT re-fire
	// every frame. WITHOUT this (default policy + a no-trigger IA), the input re-triggered ~every frame and
	// climb re-activated repeatedly (the "robot repeatedly lunges into the wall" symptom). The IA keeps its
	// InputTriggerPressed; this policy is what makes the hold a single sustained activation. The
	// WaitInputRelease task still ends it on release (nothing else calls CancelInputActivatedAbilities --
	// its sole caller is the CheatManager; same reason Beam_v2 keeps the task).
	ActivationPolicy = ELyraAbilityActivationPolicy::WhileInputActive;

	ActivationBlockedTags.AddTag(TAG_Climb_State_Match_Warmup);
	ActivationBlockedTags.AddTag(TAG_Climb_State_Match_Ended);
	ActivationBlockedTags.AddTag(TAG_Climb_State_Extracting);

	// Decision H: cannot climb while carrying an object. GAS blocks activation when the avatar's ASC holds a
	// blocked tag (the canonical, less-code path vs a CanActivateAbility override). The carry component
	// separately force-drops the held object if a climb somehow starts -> carry + climb are a clean either/or.
	ActivationBlockedTags.AddTag(TAG_Climb_State_Carrying);
}

void UAFLGameplayAbility_Climb::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Reset ALL per-activation state. This ability is InstancedPerActor -> the SAME instance is reused for
	// every climb, so any flag left set from the previous climb persists. The bug this fixes: bMantling stayed
	// true after the first climb's mantle, so on climb #2+ TryMantleOrExit early-returned on the
	// (bExiting || bMantling) guard -> the mantle never fired -> the loop ran until something interrupted it
	// (reason=interrupted). Clearing both flags + the stale task handle here makes every climb behave like the first.
	bExiting = false;
	bMantling = false;
	bOrientationApplied = false;
	MontageTask = nullptr;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}

	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: Activate by %s."), *GetNameSafe(Character));

	// 1. Surface validation: forward trace; no wall -> not a valid climb, cancel.
	if (const UWorld* World = Character->GetWorld())
	{
		const FVector Start = Character->GetActorLocation();
		const FVector End = Start + Character->GetActorForwardVector() * SurfaceTraceDistance;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLClimbActivateTrace), false, Character);
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		if (!bHit)
		{
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: no surface within %.0fcm -> cancel."), SurfaceTraceDistance);
			CancelAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateCancelAbility*/ true);
			return;
		}
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: surface valid (normal=%s)."), *Hit.ImpactNormal.ToCompactString());

		// ORIENTATION via MOTIONWARPING (Cycle 4a -- the foundation pattern, replacing the manual
		// SetActorRotation+TeleportPhysics lerp). The climb anim is authored for a character FACING the wall;
		// rather than snap the actor yaw (which doesn't scale to per-object/variable targets and isn't net-clean),
		// we set a named warp target the montage's MotionWarping notify window skews the root motion toward.
		// WarpToWall = the wall surface point + facing INTO the wall (-ImpactNormal, yaw only). The notify window
		// over Section_Loop applies it through the ascent; EndAbility clears it. MotionWarping replicates the
		// target out of the box (multiplayer-correct).
		if (UMotionWarpingComponent* MotionWarping = Character->FindComponentByClass<UMotionWarpingComponent>())
		{
			// The component ctor leaves bSearchForWindowsInAnimsWithinMontages=false; our warp windows live on
			// the montage's sub-anims, so enable it (idempotent each activation).
			MotionWarping->bSearchForWindowsInAnimsWithinMontages = true;

			FVector FaceDir = -Hit.ImpactNormal;
			FaceDir.Z = 0.0f;
			FaceDir.Normalize();
			const FVector WarpLocation = Hit.ImpactPoint;
			const FRotator WarpRotation = FaceDir.Rotation();
			MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(FName("WarpToWall"), WarpLocation, WarpRotation);
			bOrientationApplied = true;
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: warp target added (name=WarpToWall, loc=%s, rot=%s)."),
				*WarpLocation.ToCompactString(), *WarpRotation.ToCompactString());
		}
		else
		{
			UE_LOG(LogAFLMovement, Warning, TEXT("AFL_CLIMB: no MotionWarpingComponent on hero -> wall alignment will not apply."));
		}
	}

	// 2. Apply the climb-active GE -> grants State.Movement.Climbing -> UAFLClimbMovementComponent flips
	//    the stock CMC to gravity-0 / MOVE_Flying. EndAbility removes it (tag clears -> CMC restored).
	if (ClimbActiveEffectClass)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(ClimbActiveEffectClass, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}

	// 3. Bind the component's surface-loss delegate (exit when the wall is lost mid-climb).
	ClimbComponent = Character->FindComponentByClass<UAFLClimbMovementComponent>();
	if (ClimbComponent.IsValid())
	{
		SurfaceLostHandle = ClimbComponent->OnClimbSurfaceLost.AddUObject(this, &UAFLGameplayAbility_Climb::OnSurfaceLost);
	}

	// 4. Input-release listener -> cancel on release.
	// bTestAlreadyReleased=FALSE: do NOT fire on the initial-state check. With =true the task's Activate()
	// immediately fired OnReleaseCallback when GAS read the input as not-currently-held on that frame, so
	// EVERY climb exited reason=input-release on the first frame (never reaching montage-complete or
	// surface-loss). =false waits for a genuine InputReleased replicated event -> the other exit paths can win.
	if (UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased*/ false))
	{
		ReleaseTask->OnRelease.AddDynamic(this, &UAFLGameplayAbility_Climb::OnInputReleased);
		ReleaseTask->ReadyForActivation();
	}

	// 5. Play the root-motion climb montage STARTING ON THE ASCENT LOOP. The loop sustains while held; the
	//    mantle cap is reached ONLY by OnInputReleased -> Montage_JumpToSection(Mantle) when a ledge top is
	//    detected. So the loop section's natural OnCompleted is effectively unreachable (it loops); the
	//    OnCompleted we receive is the MANTLE cap finishing (bMantling==true) -> reason=complete.
	if (ClimbMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, ClimbMontage, /*Rate*/ 1.0f, /*StartSection*/ LoopSectionName, /*bStopWhenAbilityEnds*/ true);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UAFLGameplayAbility_Climb::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UAFLGameplayAbility_Climb::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UAFLGameplayAbility_Climb::OnMontageInterruptedOrCancelled);
			MontageTask->OnCancelled.AddDynamic(this, &UAFLGameplayAbility_Climb::OnMontageInterruptedOrCancelled);
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: montage start (%s, section=%s)."),
				*GetNameSafe(ClimbMontage), *LoopSectionName.ToString());
			MontageTask->ReadyForActivation();
		}
	}
	else
	{
		// No montage authored yet -> the climb-state GE still applied; without root motion there is no
		// up-translation, so we rely on input-release / surface-loss to exit. Log so this is visible.
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: no ClimbMontage set -> CMC state applied, no root motion (placeholder pending)."));
	}
}

void UAFLGameplayAbility_Climb::OnMontageCompleted()
{
	// Reached when the montage finishes/blends out. In the E2 design the LOOP section loops indefinitely, so
	// the only completion we expect is the MANTLE cap finishing (bMantling==true) -> the success exit. If we
	// somehow complete without mantling (e.g. a non-looping montage, or a future single-section authoring),
	// still treat it as success -- but log which path so a pop is attributable.
	if (bMantling)
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: mantle cap complete -> character on ledge."));
		// Happy path beat the fallback timer -> cancel it so it can't double-fire.
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(MantleTimeoutHandle);
		}
	}
	else
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: montage completed without mantle (loop not sustaining / single-section)."));
	}
	ExitClimb(TEXT("complete"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_Climb::OnMontageInterruptedOrCancelled()
{
	ExitClimb(TEXT("interrupted"), /*bCancelled*/ true);
}

void UAFLGameplayAbility_Climb::OnInputReleased(float TimeHeld)
{
	// Release-near-top -> mantle cap; release-mid-wall -> drop (the b96c025d behavior). The ledge-top test
	// here is the UPWARD trace (clear above == near a top); surface-loss (below) is the OTHER top signal.
	if (bExiting || bMantling)
	{
		return; // already handed off or exiting; ignore a duplicate release.
	}

	if (IsLedgeTopClear())
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: input released, ledge top clear above -> mantle."));
		TryMantleOrExit(TEXT("input-release"));
		return;
	}

	// Wall continues above -> the player let go mid-wall on purpose; drop.
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: input released, wall continues above (upward trace blocked) -> drop."));
	ExitClimb(TEXT("input-release"), /*bCancelled*/ true);
}

bool UAFLGameplayAbility_Climb::IsLedgeTopClear() const
{
	const ACharacter* Character = GetCurrentActorInfo() ? Cast<ACharacter>(GetCurrentActorInfo()->AvatarActor.Get()) : nullptr;
	const UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		return false;
	}

	// Trace straight UP from the avatar origin. CLEAR (no hit) = at/near a ledge top -> mantle is valid.
	// BLOCKED (hit) = the wall continues above -> not a top, drop instead.
	const FVector Start = Character->GetActorLocation();
	const FVector End = Start + FVector::UpVector * LedgeTopTraceDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLClimbLedgeTopTrace), /*bTraceComplex*/ false, Character);
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	return !bBlocked;
}

void UAFLGameplayAbility_Climb::OnSurfaceLost()
{
	// KEY E2 FIX. The component's forward wall-trace failing is NOT a failure -- after ascending, losing the
	// forward wall means the character has climbed PAST THE TOP of the wall. That is the ledge-top arrival
	// signal (more reliable than the upward trace, which the wall's own top lip can fool). So surface-loss
	// FIRES THE MANTLE, not a drop. We only drop here if no mantle cap is available (single-section montage)
	// or we're already exiting/mantling. This collapses the race that was killing every climb at the top:
	// the per-tick surface-loss watchdog used to win and abort before input-release could fire the mantle.
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: forward wall lost (climbed past top) -> mantle."));
	TryMantleOrExit(TEXT("surface-loss"));
}

void UAFLGameplayAbility_Climb::TryMantleOrExit(const TCHAR* FallbackReason)
{
	if (bExiting || bMantling)
	{
		return; // a prior signal already handed off or began exiting; first one wins.
	}

	// Fire the mantle cap if the montage actually has a distinct mantle section to jump to.
	if (MontageTask && ClimbMontage && !MantleSectionName.IsNone() &&
		ClimbMontage->IsValidSectionName(MantleSectionName))
	{
		bMantling = true;
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: firing mantle cap (section=%s)."), *MantleSectionName.ToString());

		// Pin gravity OFF for the mantle so its root motion drives the up+forward arc onto the ledge without
		// the CMC's gravity yanking the character back down mid-cap. RestoreClimbState (on EndAbility) returns
		// gravity to normal once the character is standing on top.
		if (ACharacter* Character = GetCurrentActorInfo() ? Cast<ACharacter>(GetCurrentActorInfo()->AvatarActor.Get()) : nullptr)
		{
			if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
			{
				CMC->GravityScale = 0.0f;
				CMC->SetMovementMode(MOVE_Flying);
			}

			// WarpToLedgeTop (Cycle 4a): set the mantle warp target so the Section_Mantle notify window skews the
			// root motion to land the character on the actual ledge surface. Find the ledge top by tracing DOWN
			// from a point above+forward of the character (where the ledge surface should be). Keep the current
			// facing for the mantle (the WarpToWall facing already turned the body into the wall). The detection
			// that we reached the top is UNCHANGED (forward-wall-lost / input-release-near-top) -- only the
			// alignment consequence is now MotionWarping instead of a SetActorRotation.
			if (UMotionWarpingComponent* MotionWarping = Character->FindComponentByClass<UMotionWarpingComponent>())
			{
				const UWorld* World = Character->GetWorld();
				const FVector Fwd = Character->GetActorForwardVector();
				const FVector Loc = Character->GetActorLocation();
				const FRotator MantleRotation = Character->GetActorRotation(); // keep wall-facing yaw

				// Find the ledge SURFACE just over the lip: trace DOWN from a point forward+above the character.
				// Probe span is short and near the character's own height so it can't pick up the distant floor.
				const FVector ProbeTop = Loc + Fwd * 50.0f + FVector::UpVector * 60.0f;
				const FVector ProbeBot = ProbeTop - FVector::UpVector * 120.0f;
				FVector LedgeTopLocation = Loc + Fwd * 50.0f; // default: forward at the CURRENT height (never below)
				bool bFoundLedge = false;
				if (World)
				{
					FCollisionQueryParams P(SCENE_QUERY_STAT(AFLClimbLedgeSurface), false, Character);
					FHitResult TopHit;
					if (World->LineTraceSingleByChannel(TopHit, ProbeTop, ProbeBot, ECC_Visibility, P))
					{
						// Only accept a hit that is at/above the character's feet -- a "ledge top" is never BELOW
						// the climber. This rejects the greybox floor far below (the Z=-0.5 "mantle at bottom" bug).
						if (TopHit.ImpactPoint.Z >= Loc.Z - 20.0f)
						{
							LedgeTopLocation = TopHit.ImpactPoint;
							bFoundLedge = true;
						}
					}
				}
				// Hard floor: never let the target sit below the character (mantle is up+forward, never down).
				LedgeTopLocation.Z = FMath::Max(LedgeTopLocation.Z, Loc.Z);

				MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(FName("WarpToLedgeTop"), LedgeTopLocation, MantleRotation);
				UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: warp target added (name=WarpToLedgeTop, loc=%s, rot=%s, foundLedge=%d)."),
					*LedgeTopLocation.ToCompactString(), *MantleRotation.ToCompactString(), bFoundLedge ? 1 : 0);
			}
		}

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->CurrentMontageJumpToSection(MantleSectionName);
		}

		// GUARANTEED EXIT. The jump-to-section can be issued while the loop is mid-blend, in which case the
		// mantle section's OnMontageCompleted is sometimes swallowed -> the ability would hang in MOVE_Flying
		// and the character floats up forever (observed on the 2nd climb). Arm a fallback timer for the mantle
		// section's length (+ buffer); whichever of OnMontageCompleted / this timer fires first ends the climb.
		float MantleLen = 3.0f; // safe default if the section length can't be read
		if (const UWorld* World = GetWorld())
		{
			const int32 SecIdx = ClimbMontage->GetSectionIndex(MantleSectionName);
			if (SecIdx != INDEX_NONE)
			{
				const float SecLen = ClimbMontage->GetSectionLength(SecIdx);
				if (SecLen > 0.0f)
				{
					MantleLen = SecLen;
				}
			}
			World->GetTimerManager().SetTimer(
				MantleTimeoutHandle, this, &UAFLGameplayAbility_Climb::OnMantleTimeout, MantleLen + 0.25f, /*loop*/ false);
		}
		return;
	}

	// No mantle cap available -> fall back to the caller's drop reason.
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: no mantle section -> exit (reason=%s)."), FallbackReason);
	ExitClimb(FallbackReason, /*bCancelled*/ true);
}

void UAFLGameplayAbility_Climb::OnMantleTimeout()
{
	// The mantle section's OnMontageCompleted never arrived within its length+buffer. Force the success exit
	// so the character is never left floating. (If OnMontageCompleted DID fire, it already cleared this timer.)
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: mantle timeout fallback -> forcing complete."));
	ExitClimb(TEXT("complete"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_Climb::ExitClimb(const TCHAR* Reason, bool bCancelled)
{
	if (bExiting)
	{
		return; // first exit wins; the others are stale signals from tasks ending.
	}
	bExiting = true;

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: exit (reason=%s)."), Reason);

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility*/ true, bCancelled);
}

void UAFLGameplayAbility_Climb::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// MOTIONWARPING cleanup (Cycle 4a): clear the warp targets this ability added so they don't persist across
	// activations (basic climb adds WarpToWall; climb-with-mantle adds WarpToLedgeTop too -- RemoveAllWarpTargets
	// handles both). Replaces the old manual bUseControllerRotationYaw restore.
	if (bOrientationApplied)
	{
		if (const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
		{
			if (UMotionWarpingComponent* MotionWarping = Character->FindComponentByClass<UMotionWarpingComponent>())
			{
				MotionWarping->RemoveAllWarpTargets();
				UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: warp targets cleaned."));
			}
		}
		bOrientationApplied = false;
	}

	// Unbind the surface-loss delegate before the component/ability tear down.
	if (ClimbComponent.IsValid() && SurfaceLostHandle.IsValid())
	{
		ClimbComponent->OnClimbSurfaceLost.Remove(SurfaceLostHandle);
	}
	SurfaceLostHandle.Reset();
	ClimbComponent.Reset();

	// Clear the mantle fallback timer on every exit path (it may already have fired or been cleared).
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MantleTimeoutHandle);
	}

	// Remove the climb-active GE so State.Movement.Climbing clears -> the component restores the CMC.
	// (Auto-removed too when the ability ends if the GE was applied with this as the source, but explicit
	// removal here guarantees the tag clears immediately on every exit path.)
	if (ClimbActiveEffectClass && ActorInfo)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(ClimbActiveEffectClass, ASC);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

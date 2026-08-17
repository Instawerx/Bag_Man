// Copyright C12 AI Gaming. All Rights Reserved.

#include "Interaction/AFLFullBodyIKComponent.h"

#include "AFLMovement.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cook/AFLCookedAssetRegistry.h"
#include "GameFramework/Character.h"

// Declared AND enrolled for cook validation in one statement. This exact path is why FBIK --
// doctrine on every character -- had never once shipped in a packaged build: nothing in the
// content graph references it, so the cooker never saw it and never packaged it, and the failure
// could not reproduce in PIE. Fixed in the cook config by 53d99a37; enrolled here so the next
// path added beside it is checked at startup instead of at BeginPlay, minutes later.
AFL_COOKED_ASSET(GProModFBIKPostProcess,
	TEXT("/Game/BagMan/ProMod/ABP_ProMod_FBIK_PP.ABP_ProMod_FBIK_PP_C"));

/**
 * FBIK kill switch. Default ON -- FBIK is doctrine, not an option.
 *
 * It exists because 53d99a37 made FBIK ship for the FIRST TIME and the first cooked run with it live froze
 * characters in T-pose. Until that is root-caused there must be a way to get a playable build WITHOUT
 * un-cooking the asset again: un-cooking removes the thing from the package entirely, which is how it stayed
 * broken and invisible for months. A runtime switch keeps it shipped, keeps it loadable, and lets the same
 * binary be run both ways -- which is exactly what isolating the fault needs.
 *
 * ⚠ NOT A FIX, AND NOT A PLACE TO STOP. Measured: NaN transforms occur at ~10/sec with FBIK absent and
 * ~123/sec with it live, so FBIK AMPLIFIES a pre-existing fault rather than causing it. Turning this off
 * hides the amplifier and leaves the cause running. See Docs/FBIK_NAN_ROOT_CAUSE.md.
 */
static TAutoConsoleVariable<int32> CVarAFLFBIKEnable(
	TEXT("afl.FBIK.Enable"),
	1,
	TEXT("1 = apply the Pro full-body-IK post-process ABP (default, doctrine). 0 = skip it. ")
	TEXT("A diagnostic escape hatch while the NaN fault is root-caused -- NOT a fix."),
	ECVF_Default);

UAFLFullBodyIKComponent::UAFLFullBodyIKComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Default to the Pro FBIK post-process ABP (the generated anim-instance class).
	PostProcessABP = GProModFBIKPostProcess.ToSoftClassPtr<UAnimInstance>();
}

void UAFLFullBodyIKComponent::BeginPlay()
{
	Super::BeginPlay();

	if (CVarAFLFBIKEnable.GetValueOnGameThread() == 0)
	{
		// Logged at Warning, deliberately. A character silently running without doctrine IK is the state that
		// went unnoticed for months; if FBIK is off, the log should say so every single time.
		UE_LOG(LogAFLMovement, Warning,
			TEXT("AFL_FBIK: DISABLED by afl.FBIK.Enable=0 -- no full-body IK on %s. Diagnostic only."),
			*GetNameSafe(GetOwner()));
		return;
	}

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

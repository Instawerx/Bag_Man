// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Laser_Pulse.h"

#include "AFLCombat.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Effects/GE_AFL_Damage_Pulse.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "LagComp/AFLLagCompensationWorldSubsystem.h"
#include "NativeGameplayTags.h"
#include "Targeting/AFLAbilityTargetData_Hitscan.h"
#include "Telemetry/AFLCombatTelemetry.h"
#include "Tuning/AFLPulseTuningData.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_Laser_Pulse)

// Native tag declarations. CDO construction runs at module load — before
// per-plugin Tags/*.ini scans complete — so FGameplayTag::RequestGameplayTag
// here would ensure-fail with "tag not found" and crash the editor on a
// fresh boot. UE_DEFINE_GAMEPLAY_TAG_STATIC registers these at module init,
// strictly before any CDO of a class in this module is constructed. The
// ini still declares the same tags as the spec source-of-truth; UE
// dedups native+ini registrations.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Laser_Pulse, "Ability.Laser.Pulse");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Firing_Pulse, "State.Firing.Pulse");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_Pulse, "State.Carrying");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_ThrowRecovery_Pulse, "State.Weapon.ThrowRecovery");

// AFL-0301: muzzle cue fired once per local shot from ClientPredictAndSend.
// File-specific symbol suffix matches the HYG-001 / AFLCombat Unity-build
// pattern (e.g. TAG_State_Overheated_Cheats vs _AttrSet vs _Beam) -- the
// FName value is the canonical tag string, the C++ symbol stays per-file
// unique to avoid Unity-merged-TU duplicate-symbol errors.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayCue_Weapon_Pulse_Fire_PulseAbility, "GameplayCue.Weapon.Pulse.Fire");

// AFL-0302: tracer cue fired post-trace from OnTargetDataReadyCallback once
// per locally-controlled real shot. Receiver is GCN_AFL_Pulse_Tracer
// (GameplayCueNotify_Burst with an On Burst BP override that drives
// NS_AFL_Pulse_Tracer's User parameters: MuzzlePosition, ImpactPositions
// (UNiagaraDataInterfaceArrayFloat3, single-element array with Hit.ImpactPoint),
// Color, Trigger). Same _PulseAbility file-specific suffix as Fire above.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayCue_Weapon_Pulse_Tracer_PulseAbility, "GameplayCue.Weapon.Pulse.Tracer");

// SetByCaller magnitude tags consumed by UAFLDamageExecCalc::Execute_Implementation
// step 2. File-specific suffix on the C++ symbol (the FName *value* stays as the
// canonical "Data.Damage.*" string). Required because UBT Unity builds merge
// multiple .cpp files into one translation unit, and anonymous namespaces
// collapse into a single TU-level namespace under that merge. Per-file rename
// is the minimal Unity-safe pattern — see UAFLGameplayAbility_DamageTest for
// the identical workaround.
namespace
{
	const FName NAME_Data_Damage_Headshot_LaserPulse  = TEXT("Data.Damage.Headshot");
	const FName NAME_Data_Damage_Weakpoint_LaserPulse = TEXT("Data.Damage.Weakpoint");
	const FName NAME_Data_Damage_Distance_LaserPulse  = TEXT("Data.Damage.Distance");
}

// BM-0105c lag-comp COMPENSATION test harness. Forces the rewind RTT so the
// distinguishing experiment runs deterministically in single-client PIE
// (no networking confounds): afl.LagComp.ForceRTT 0.2 rewinds to a past pose,
// afl.LagComp.ForceRTT 0 stays at "now" — same aim, verdict flips, isolating
// rewind as the cause. -1 = use real ping (production path). The forced value
// still passes through the same 0.2s clamp as the real path (master doc 7.4),
// so ForceRTT=0.2 exercises the system at its designed max compensation.
// ECVF_Cheat + shipping-guarded: never present in a shipping build.
#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<float> CVarAFLLagCompForceRTT(
	TEXT("afl.LagComp.ForceRTT"),
	-1.0f,
	TEXT("Debug: force lag-comp RTT in seconds. -1 = use real ping. >=0 = forced. Clamped to 0.2 like the real path."),
	ECVF_Cheat);
#endif


UAFLAG_Laser_Pulse::UAFLAG_Laser_Pulse()
{
	// Locally-predicted, instanced-per-actor. Matches the master doc Sec. 6/7
	// contract for client-authoritative hitscan: the firing client predicts
	// activation and the trace (AFL-0106), then ships TargetData to the server.
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// AbilityTags advertise this ability's identity (granted-by-class lookup
	// in DA_AFL_AbilitySet_*, and used by ActivationOwnedTags to apply
	// State.Firing.Pulse for the lifetime of the activation).
	AbilityTags.AddTag(TAG_Ability_Laser_Pulse);
	ActivationOwnedTags.AddTag(TAG_State_Firing_Pulse);

	// Throw cycle: while CARRYING an object, the shared LMB belongs to UAFLGameplayAbility_Throw
	// (ActivationRequiredTags=State.Carrying); this block is the other half of that arbitration -- the
	// holstered (hidden but still equipped) rifle must not fire under the carry. Tag clears on release.
	ActivationBlockedTags.AddTag(TAG_State_Carrying_Pulse);

	// ...and the press that THREW must not fire either: the throw applies GE_AFL_ThrowRecovery (0.4s)
	// granting this tag, covering the post-throw frames where the carry tag is already gone.
	ActivationBlockedTags.AddTag(TAG_State_ThrowRecovery_Pulse);

	// Cooldown tag declared in AFLCombatTags.ini (Cooldown.Weapon.Pulse). The
	// concrete Cooldown GE is wired by the AbilitySet data asset in AFL-0214.
	// Cost GE is a placeholder until heat lands in AFL-0207; left unset here.

	// AFL-0105: default to the native pulse damage GE. BP children of the
	// ability can still override this on the CDO in AFL-0214 if they need a
	// designer-tuned variant.
	DamageEffectClass = UGE_AFL_Damage_Pulse::StaticClass();

	// AFL-0209 (BM-0104): native default ship-tuning. Per-shot ClientPredictAndSend
	// re-reads this DA every shot, so live edits in the Content Browser propagate
	// without restart. The Succeeded() guard means a missing/renamed asset
	// gracefully falls through to the hardcoded fallback constants at
	// ClientPredictAndSend (which match DA_AFLPulseTuning's defaults by design).
	// BP children may override per Pulse.h:53-54.
	static ConstructorHelpers::FObjectFinder<UAFLPulseTuningData> TuningFinder(
		TEXT("/AFLCombat/Tuning/DA_AFLPulseTuning.DA_AFLPulseTuning"));
	if (TuningFinder.Succeeded())
	{
		TuningData = TuningFinder.Object;
	}
}

void UAFLAG_Laser_Pulse::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// Cost/cooldown check failed (cooldown active, insufficient heat once
		// AFL-0207 wires it, etc.). CommitAbility cancels the prediction key
		// for us; just bail.
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// 2-client watch instrumentation: stamp WHO fired (avatar name distinguishes PIE instances
	// C_0=host vs C_1=client) + net role + locally-controlled. On a listen-host the client's
	// predicted Activate and the server's re-run both land in one combined log, so the avatar
	// name + role together are what tell the two windows apart. The whole c0/c1/lag-comp pass
	// turns on "which instance did this" -- a bare "Activate" can't answer it.
	{
		const AActor* AvatarActor = GetAvatarActorFromActorInfo();
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_PULSE: Activate by %s (role=%d localctrl=%d)"),
			*GetNameSafe(AvatarActor),
			AvatarActor ? static_cast<int32>(AvatarActor->GetLocalRole()) : 0,
			ActorInfo->IsLocallyControlled() ? 1 : 0);
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	// Bind the target-data delegate on BOTH sides:
	//   * Client: fires immediately from ClientPredictAndSend (local).
	//   * Server: fires when the replicated TargetData arrives from
	//     ServerSetReplicatedTargetData. ServerApplyTargetData reads it
	//     and applies DamageEffectClass to the hit target.
	// EndAbility unbinds via the saved delegate handle.
	OnTargetDataReadyCallbackDelegateHandle =
		ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
		   .AddUObject(this, &ThisClass::OnTargetDataReadyCallback);

	if (ActorInfo->IsLocallyControlled())
	{
		// Local predicting client (or listen-server host): build the target data
		// from the camera and dispatch. Server-only avatars (dedicated server
		// sim proxies of someone else's pawn) don't trace — they sit on the
		// delegate above and wait for the replicated TargetData.
		ClientPredictAndSend();
	}
}

void UAFLAG_Laser_Pulse::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (ScopeLockCount > 0)
		{
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(
				this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
			return;
		}

		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
			ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
			   .Remove(OnTargetDataReadyCallbackDelegateHandle);
			ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
		}

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

FVector UAFLAG_Laser_Pulse::ResolveMuzzleLocation(APawn* AvatarPawn) const
{
	// Fallback to weapon_r hand socket -- the cue never spawns at origin when
	// the muzzle socket can't be found (un-armed, or a future weapon w/o the
	// "Muzzle" socket convention).
	FVector MuzzleLocation = FVector::ZeroVector;
	if (!AvatarPawn)
	{
		return MuzzleLocation;
	}

	if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
	{
		if (USkeletalMeshComponent* CharMesh = AvatarChar->GetMesh())
		{
			MuzzleLocation = CharMesh->GetSocketLocation(FName("weapon_r"));
		}
	}

	// Path A: pawn->GetAttachedActors (root-attached weapons).
	TArray<AActor*> AttachedActors;
	AvatarPawn->GetAttachedActors(AttachedActors);
	UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_PULSE/MUZZLE: pawn->GetAttachedActors returned %d"), AttachedActors.Num());
	for (AActor* Attached : AttachedActors)
	{
		TInlineComponentArray<UStaticMeshComponent*> SMCs;
		Attached->GetComponents<UStaticMeshComponent>(SMCs);
		UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_PULSE/MUZZLE:  attached=%s SMCs=%d"), *Attached->GetName(), SMCs.Num());
		bool bFound = false;
		for (UStaticMeshComponent* SMC : SMCs)
		{
			if (SMC && SMC->DoesSocketExist(FName("Muzzle")))
			{
				MuzzleLocation = SMC->GetSocketLocation(FName("Muzzle"));
				UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_PULSE/MUZZLE:   FOUND on SMC=%s at world=%s"), *SMC->GetName(), *MuzzleLocation.ToString());
				bFound = true;
				break;
			}
		}
		if (bFound) return MuzzleLocation;
	}

	// Path B: fallback -- walk the character mesh's attached actors (Lyra's equipment
	// attaches the weapon to Char->GetMesh(), NOT to the pawn root -- so pawn->Get-
	// AttachedActors above returns empty for equipped weapons). Try the mesh too.
	if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
	{
		if (USkeletalMeshComponent* CharMesh = AvatarChar->GetMesh())
		{
			TArray<USceneComponent*> MeshChildren;
			CharMesh->GetChildrenComponents(/*bIncludeAllDescendants=*/true, MeshChildren);
			UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_PULSE/MUZZLE: mesh->GetChildrenComponents returned %d"), MeshChildren.Num());
			for (USceneComponent* Child : MeshChildren)
			{
				if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
				{
					if (SMC->DoesSocketExist(FName("Muzzle")))
					{
						MuzzleLocation = SMC->GetSocketLocation(FName("Muzzle"));
						UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_PULSE/MUZZLE:  FOUND via mesh-child SMC=%s at world=%s"), *SMC->GetName(), *MuzzleLocation.ToString());
						return MuzzleLocation;
					}
				}
			}
		}
	}

	UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_PULSE/MUZZLE: NOT FOUND -- falling back to weapon_r at %s"), *MuzzleLocation.ToString());
	return MuzzleLocation;
}

void UAFLAG_Laser_Pulse::ClientPredictAndSend()
{
	check(CurrentActorInfo);

	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn)
	{
		return;
	}

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	// AFL-0209 tuning constants. Resolved once per shot from the data asset,
	// with a hardcoded fallback matching UAFLPulseTuningData's defaults so the
	// ability never crashes on a missing DA. Keeping these as locals (vs
	// inlining TuningData->* in the math) keeps the null-guard cost to one
	// branch and makes the SetSpread/SetRecoil hot-swap path naturally
	// atomic — the cheats overwrite TuningData, and the next shot picks up
	// the new values on its next resolve.
	const UAFLPulseTuningData* Tuning = TuningData;
	const float BaseSpread             = Tuning ? Tuning->BaseSpreadDegrees       : 0.5f;
	const float MaxSpread              = Tuning ? Tuning->MaxSpreadDegrees        : 4.0f;
	const float SpreadPerShot          = Tuning ? Tuning->SpreadPerShotDegrees    : 0.6f;
	const float SpreadRecoveryPerSec   = Tuning ? Tuning->SpreadRecoveryDegPerSec : 8.0f;
	const float RecoilPitch            = Tuning ? Tuning->RecoilPitchPerShot      : 0.4f;
	const float RecoilYawJitter        = Tuning ? Tuning->RecoilYawJitterDegrees  : 0.15f;

	// Camera-aligned origin and direction. PlayerCameraManager exposes the same
	// post-modifier viewpoint as the controller's view helper, without tripping
	// the AFL-0215 lint rule (which is intentionally a blanket grep — the view
	// helper is treated as a smell anywhere in AFLCombat). Falls back to the
	// controller's control rotation + pawn eye location for AI-controlled
	// pawns or split-screen edge cases where PlayerCameraManager is absent.
	FVector  ViewLocation = AvatarPawn->GetPawnViewLocation();
	FRotator ViewRotation = AvatarPawn->GetViewRotation();

	APlayerController* PC = Cast<APlayerController>(AvatarPawn->GetController());
	if (PC)
	{
		if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
		{
			ViewLocation = CamMgr->GetCameraLocation();
			ViewRotation = CamMgr->GetCameraRotation();
		}
	}

	const FVector AimDirection = ViewRotation.Vector().GetSafeNormal();

	// AFL-0209 bloom: decay toward Base by SpreadRecoveryPerSec * dt since the
	// last shot, then add this shot's bump. Result feeds VRandCone for both the
	// trace and the replicated ClaimedAimDirection so the server's lag-comp
	// re-trace stays consistent with the client's perturbed shot.
	UWorld* World = AvatarPawn->GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	// One-time floor sync: on the very first activation, snap CurrentSpread to
	// the RESOLVED BaseSpread (from the DA, or the null-guarded default). The
	// header initializer can't see TuningData, so without this gate the first
	// shot would bloom from the header literal regardless of how the designer
	// tuned the DA.
	if (!bBloomInitialized)
	{
		CurrentSpreadDegrees = BaseSpread;
		bBloomInitialized = true;
	}

	const float DeltaSinceLastFire = FMath::Max(0.0f, Now - LastFireTime);
	CurrentSpreadDegrees = FMath::Clamp(
		CurrentSpreadDegrees - SpreadRecoveryPerSec * DeltaSinceLastFire,
		BaseSpread,
		MaxSpread);
	CurrentSpreadDegrees = FMath::Min(CurrentSpreadDegrees + SpreadPerShot, MaxSpread);
	LastFireTime = Now;

	// AFL-0209 PIE-validation log. Verbose so it doesn't spam shipping/release;
	// `log LogAFLCombat Verbose` in the console surfaces it during the bloom
	// regression test. Once a/b are validated this can drop to a one-time
	// "AFL_PULSE: Bloom init" line or be removed entirely.
	UE_LOG(LogAFLCombat, Verbose,
		TEXT("AFL_PULSE: Bloom cur=%.2f base=%.2f max=%.2f dt=%.3f"),
		CurrentSpreadDegrees, BaseSpread, MaxSpread, DeltaSinceLastFire);

	const FVector PerturbedDirection = FMath::VRandCone(
		AimDirection,
		FMath::DegreesToRadians(CurrentSpreadDegrees));
	const FVector EndTrace = ViewLocation + PerturbedDirection * MaxRange;

	// AFL-0301: the muzzle (Fire) cue is NO LONGER fired here. It moved to the unified, role-agnostic
	// cosmetic block in OnTargetDataReadyCallback (inside the shared prediction window), alongside the
	// tracer/impact cue -- so all three cosmetics fire ONCE through the predicted path and GAS dedups
	// the owner / shows proxies via the shared key (Lyra GA_Weapon_Fire pattern). Firing it BOTH here
	// AND in the callback would double the muzzle flash on the owner. The muzzle world-point this block
	// used to resolve is now packed into ClaimedMuzzleLocation below (same ResolveMuzzleLocation call),
	// and the callback fires the cue from that payload point on every side.

	// Single-bullet line trace. The Lyra RangedWeapon pattern adds spread,
	// sweep radius, and a pawn-pass filter — Pulse is a single tight beam so
	// the simpler trace is the correct match. Spread/sweep extensions land
	// when Beam (AFL-0124) needs them.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLPulseHitscan), /*bTraceComplex=*/true);
	Params.AddIgnoredActor(AvatarPawn);
	Params.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	if (World)
	{
		World->LineTraceSingleByChannel(Hit, ViewLocation, EndTrace, TraceChannel, Params);
	}

	// Ensure the hit has a sane trace direction even when the trace misses
	// the world entirely (no actor downstream — the server will still skip
	// damage apply, but listeners may want the impact point for cosmetic FX).
	if (!Hit.bBlockingHit)
	{
		Hit.TraceStart = ViewLocation;
		Hit.TraceEnd   = EndTrace;
		Hit.Location   = EndTrace;
		Hit.ImpactPoint = EndTrace;
	}

	// Pack into the AFL target-data struct. NewTargetData ownership transfers
	// to the FGameplayAbilityTargetDataHandle via Add(); the handle's destructor
	// deletes it. Heap-allocation is the convention for target-data structs
	// because the handle stores them as TSharedPtr<FGameplayAbilityTargetData>.
	FAFLAbilityTargetData_Hitscan* NewTargetData = new FAFLAbilityTargetData_Hitscan();
	NewTargetData->HitResult                  = Hit;
	NewTargetData->ClaimedViewOrigin          = ViewLocation;
	NewTargetData->ClaimedAimDirection        = PerturbedDirection;
	NewTargetData->AimAngularVelocityDegPerSec = 0.0f; // AFL-0213 measurement lands with the input plumbing.
	// Pack the OWNER-resolved muzzle (same ResolveMuzzleLocation the Fire cue uses above; log-confirmed
	// correct on role=2). The authoritative Fire cue on the server reads THIS instead of resolving the
	// muzzle server-side, so the proxy's flash sits at the correct barrel tip. COSMETIC-only.
	NewTargetData->ClaimedMuzzleLocation      = ResolveMuzzleLocation(AvatarPawn);

	FGameplayAbilityTargetDataHandle TargetDataHandle;
	TargetDataHandle.Add(NewTargetData);

	// AFL-0209 recoil. Owning-client-only, cosmetic, additive via the input
	// chain (NEVER a control-rotation write — never replicated). AddPitchInput
	// is sign-inverted because the input system treats positive pitch as look-
	// down; we want a kick up. Guard on IsLocallyControlled() so listen-server
	// hosts kick their own camera but DON'T kick remote pawns' authoritative
	// view (they don't own those controllers in the first place — the if-check
	// is belt-and-braces against future refactors).
	if (PC && CurrentActorInfo->IsLocallyControlled())
	{
		PC->AddPitchInput(-RecoilPitch);
		PC->AddYawInput(FMath::FRandRange(-RecoilYawJitter, RecoilYawJitter));
	}

	// Open a prediction window so the local-side OnTargetDataReadyCallback
	// runs under the same prediction key the server will see on the replicated
	// path. ServerSetReplicatedTargetData is dispatched inside the callback
	// when we detect IsLocallyControlled && !IsNetAuthority.
	{
		FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());
		OnTargetDataReadyCallback(TargetDataHandle, FGameplayTag());
	}
}

void UAFLAG_Laser_Pulse::OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag)
{
	check(CurrentActorInfo);
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	if (!ASC->FindAbilitySpecFromHandle(CurrentSpecHandle))
	{
		// Ability was cancelled out from under us between dispatch and delegate.
		return;
	}

	// SHARED PREDICTION WINDOW (Lyra-canonical, mirrors ULyraGameplayAbility_RangedWeapon::
	// OnTargetDataReadyCallback). Opened at the TOP of the callback so it is active on BOTH the
	// predicted-local invocation (from ClientPredictAndSend) AND the server's replicated invocation
	// (from the AbilityTargetDataSetDelegate). The cosmetic cues below fire INSIDE this window, so the
	// owner's predicted execution and the authority's multicast SHARE the prediction key -> GAS's
	// built-in NetMulticast_InvokeGameplayCueExecuted dedup suppresses the owner's duplicate (valid key
	// = "I predicted this") and proxies play it (key=0). This REPLACES the hand-rolled owner-K2 +
	// authority-ExecuteGameplayCue + bIsAuthority/!bIsLocallyControlled gate, which fired TWO unkeyed
	// executions -> double + mirror on the listen-host. LOAD-BEARING: without this window on the SERVER
	// path, the authority cue has no scoped key and the dedup fails -> a CLEAN DOUBLE returns. (Before
	// this change the only window was inside the bIsLocallyControlled && !bIsAuthority server-send block
	// below -- it scoped only the local RPC send, NOT the cues and NOT the server invocation.)
	// THIS IS THE SINGLE WINDOW (Lyra's one-window shape): the cosmetic cues AND the server-send below
	// both run under ITS key. The former nested send-window was REMOVED (it generated a second dependent
	// key -> cues and send under different keys). One window -> cues + send share one key, trivially.
	FScopedPredictionWindow CueScopedPrediction(ASC);

	// Take ownership of the target data so downstream game code can't invalidate
	// it under us. Pattern lifted from ULyraGameplayAbility_RangedWeapon.
	FGameplayAbilityTargetDataHandle LocalTargetDataHandle(
		MoveTemp(const_cast<FGameplayAbilityTargetDataHandle&>(InData)));

	const bool bIsAuthority       = CurrentActorInfo->IsNetAuthority();
	const bool bIsLocallyControlled = CurrentActorInfo->IsLocallyControlled();

	// UNIFIED, ROLE-AGNOSTIC COSMETIC CUES (Lyra-canonical -- mirrors GA_Weapon_Fire: a Local-Predicted
	// ability that fires GameplayCue.Weapon.*.Fire / .Impact ONCE through the prediction window, params
	// carried as cue data, NO role gate). Fires on EVERY invocation of this callback -- owner-predicted
	// AND server-replicated -- both inside the ScopedPrediction window above, so both share the key.
	// GAS then dedups: the OWNER receives the authority multicast with a VALID key -> skips it (already
	// predicted); PROXIES receive key=0 -> play it. No double, no mirror, on listen-server AND dedicated.
	// This REPLACED the hand-rolled owner-K2-only block + the bIsAuthority/!bIsLocallyControlled authority
	// branch (two unkeyed executions = the double + mirror). There is NO gate backstop now -- correctness
	// rests entirely on the shared key, which is why the ScopedPrediction window placement (both paths)
	// is load-bearing. bIsAuthority/bIsLocallyControlled remain for the server-RPC + EndAbility logic below.
	//
	// COSMETIC ONLY -- trace origin / AimDirection / Hit / damage / crosshair untouched. Fire muzzle from
	// the client-authoritative ClaimedMuzzleLocation; tracer/impact origin from ClaimedViewOrigin/
	// ClaimedAimDirection (the same payload both sides; impact rides the tracer cue's OnBurst).
	if (LocalTargetDataHandle.Num() > 0)
	{
		const FGameplayAbilityTargetData* RawData = LocalTargetDataHandle.Get(0);
		if (RawData && RawData->GetHitResult() &&
			RawData->GetScriptStruct() == FAFLAbilityTargetData_Hitscan::StaticStruct())
		{
			APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
			if (AvatarPawn)
			{
				const FAFLAbilityTargetData_Hitscan* HitscanData =
					static_cast<const FAFLAbilityTargetData_Hitscan*>(RawData);
				const FHitResult& HitRef = *RawData->GetHitResult();

				// FIRE cue (muzzle flash + audio) -- muzzle from the client-authoritative payload point.
				{
					FGameplayCueParameters FireCueParams;
					FireCueParams.Location     = HitscanData->ClaimedMuzzleLocation;
					FireCueParams.Normal       = HitscanData->ClaimedAimDirection.GetSafeNormal();
					FireCueParams.Instigator   = AvatarPawn;
					FireCueParams.SourceObject = AvatarPawn;
					K2_ExecuteGameplayCueWithParams(TAG_GameplayCue_Weapon_Pulse_Fire_PulseAbility, FireCueParams);
				}

				// TRACER cue (+ impact via its OnBurst, which reads Params.EffectContext.GetHitResult()
				// to drive NS_AFL_Pulse_Tracer's User.ImpactPositions). START = the world-space MUZZLE
				// (ClaimedMuzzleLocation), END = the real Hit.ImpactPoint via EffectContext. This is the
				// VIEWPOINT-INDEPENDENT muzzle->impact model proven by Beam_v2 (UAFLBeamVisualComponent:
				// SetWorldLocation(Muzzle) start + Beam End = ImpactPoint) -- both are real world points,
				// so the tracer renders correctly from ANY camera, including the proxy. (REPLACED the old
				// camera-ray anchor: ClaimedViewOrigin + ClaimedAimDirection * TracerVisualOriginDistance,
				// which was the SHOOTER'S camera point projected forward -- correct only from the shooter's
				// own view, floating beside the pawn on a proxy. That was the Pulse-tracer-on-proxy bug.)
				// NOTE: a slight shooter-POSE bend (weapon barrel vs aim pose) is a SEPARATE, pre-existing
				// cosmetic that also affects Beam_v2; deliberately NOT addressed here.
				{
					FGameplayCueParameters TracerParams;
					TracerParams.Location     = HitscanData->ClaimedMuzzleLocation;
					TracerParams.Instigator   = AvatarPawn;
					TracerParams.SourceObject = AvatarPawn;

					FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
					Ctx.AddHitResult(HitRef);
					TracerParams.EffectContext = Ctx;

					K2_ExecuteGameplayCueWithParams(TAG_GameplayCue_Weapon_Pulse_Tracer_PulseAbility, TracerParams);
				}
			}
		}
	}

	// Client predicting on a remote client: ship the data to the server. On
	// the listen-server host both flags are true and we skip the RPC because
	// the server-side delegate fires from this same call.
	//
	// The send runs under the OUTER CueScopedPrediction window's key (opened at the top of this
	// callback) -- NOT a nested window of its own. This is Lyra's exact shape (RangedWeapon line 484
	// single window, send under ScopedPredictionKey at line 492): ONE window, so the cosmetic cues
	// above AND this RPC ship under the SAME prediction key. A nested FScopedPredictionWindow here
	// (the previous structure) called GenerateDependentPredictionKey() to make a SECOND, dependent key
	// -> cues and send would have run under DIFFERENT keys (the nested-key hazard). Removing it makes
	// "do the cues and the send share a key?" a trivial yes -- which is what the GAS dedup needs.
	if (bIsLocallyControlled && !bIsAuthority)
	{
		ASC->CallServerSetReplicatedTargetData(
			CurrentSpecHandle,
			CurrentActivationInfo.GetActivationPredictionKey(),
			LocalTargetDataHandle,
			ApplicationTag,
			ASC->ScopedPredictionKey);
	}

#if WITH_SERVER_CODE
	if (bIsAuthority)
	{
		ServerApplyTargetData(LocalTargetDataHandle);
	}
#endif

	// Match Lyra: clear the per-key cached replicated data now that we've
	// consumed it. Safe to call on the client too — it's a no-op when there's
	// nothing cached.
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	if (bIsLocallyControlled || bIsAuthority)
	{
		// The shot is done from both viewpoints. EndAbility on the predicting
		// path closes the prediction window; the server's authoritative end
		// replicates back to clients via bReplicateEndAbility.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			/*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
	}
}

#if WITH_SERVER_CODE
void UAFLAG_Laser_Pulse::ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data)
{
	UAbilitySystemComponent* SourceASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC || !DamageEffectClass)
	{
		return;
	}

	for (int32 Index = 0; Index < Data.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* RawData = Data.Get(Index);
		if (!RawData)
		{
			continue;
		}

		// We only know how to validate our own subclass; ignore foreign payloads.
		// A real "invalid schema" reject lands in AFL-0211's ValidateTargetData.
		if (RawData->GetScriptStruct() != FAFLAbilityTargetData_Hitscan::StaticStruct())
		{
			const UScriptStruct* ActualStruct = RawData->GetScriptStruct();
			FAFLCombatTelemetry::EmitRejection(
				TEXT("schema"),
				CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr,
				FString::Printf(TEXT("struct=%s"),
					ActualStruct ? *ActualStruct->GetName() : TEXT("null")));
			continue;
		}

		const FAFLAbilityTargetData_Hitscan* HitscanData = static_cast<const FAFLAbilityTargetData_Hitscan*>(RawData);

		// AFL-0213 telemetry stub. The real budget is per-pawn and the real
		// reject (drop the shot, increment reject counter) lands with
		// UAFLLagCompensationWorldSubsystem (consumer wired in BM-0105a, this
		// block below). The angular-velocity check stays as the AFL-0213 budget
		// stub — it logs only, doesn't gate damage. The lag-comp pass IS the
		// real geometric reject path now.
		if (HitscanData->AimAngularVelocityDegPerSec > MaxAimAngularVelocityDegPerSec)
		{
			FAFLCombatTelemetry::EmitAngularAnomaly(
				CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr,
				HitscanData->AimAngularVelocityDegPerSec,
				MaxAimAngularVelocityDegPerSec);
		}

		AActor* HitActor = HitscanData->HitResult.GetActor();
		if (!HitActor)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC)
		{
			continue;
		}

		// BM-0105a: lag-comp rewind + bounding-box confirm pass.
		//
		// Subsystem is server-only and returns a valid empty token in
		// degenerate cases (client world, no registered components). With no
		// per-pawn UAFLPawnHitboxHistoryComponent grants in place yet
		// (BM-0105b's job), Token.Entries is empty for every shot in this
		// sprint — the BuildBoundingBox path returns false and we default-
		// accept. The call pair runs and emits its log line; the actual
		// geometric reject is exercised only once per-pawn snapshotting lands.
		//
		// RTT clamped at 200ms per master doc Sec. 7.4. The half (ping/2) +
		// interp term lives on the client; here we use the server-side
		// APlayerState::GetPingInMilliseconds() which returns ExactPing on the
		// server (Lyra-canonical). Defensive null-guards on PC and PS:
		// server-driven hitscan (AI bots) has no PC, and a just-spawned pawn may
		// not have a PlayerState bound yet.
		if (UWorld* World = GetWorld())
		{
			if (UAFLLagCompensationWorldSubsystem* LagComp =
				World->GetSubsystem<UAFLLagCompensationWorldSubsystem>())
			{
				APlayerController* SourcePC = CurrentActorInfo ? CurrentActorInfo->PlayerController.Get() : nullptr;
				// Ping API lives on APlayerState in UE 5.6 (PlayerController's float
				// GetPing() was removed; the canonical accessor returns ExactPing on
				// server / local, falls back to compressed-ping on remote clients).
				const APlayerState* SourcePS = SourcePC ? SourcePC->PlayerState : nullptr;
				const float RawRTT     = SourcePS ? (SourcePS->GetPingInMilliseconds() * 0.001f) : 0.0f;

				// BM-0105c: synthetic-RTT override for the compensation proof. The
				// forced value still clamps to 0.2 like the real path, so the test
				// exercises the system's designed max compensation, not a bypass.
#if !UE_BUILD_SHIPPING
				const float ForcedRTT    = CVarAFLLagCompForceRTT.GetValueOnGameThread();
				const float EffectiveRTT = (ForcedRTT >= 0.0f) ? ForcedRTT : RawRTT;
#else
				const float EffectiveRTT = RawRTT;
#endif
				const float ClampedRTT = FMath::Min(EffectiveRTT, 0.2f);

#if !UE_BUILD_SHIPPING
				if (ForcedRTT >= 0.0f)
				{
					UE_LOG(LogAFLCombat, Verbose,
						TEXT("AFL_LAGCOMP: ForceRTT active = %.3f (synthetic, not real ping)"),
						ClampedRTT);
				}
#endif

				// BM-0105c: the rewind + bounding-box + pad + verdict pass now
				// lives in one shared method on the subsystem. The afl.LagComp.TestFire
				// debug command calls the SAME ConfirmHit, so the isolated RTT-flip
				// proof exercises this exact shipping path (not a reimplementation).
				// ConfirmHit emits the "rewind dt=... entries=... verdict=..." log and
				// owns the rewind/restore internally; it takes the DELTA (ClampedRTT).
				if (!LagComp->ConfirmHit(SourcePC, ClampedRTT, HitActor, HitscanData->HitResult.ImpactPoint))
				{
					UE_LOG(LogAFLCombat, Verbose,
						TEXT("AFL_LAGCOMP: hitscan_reject reason=geometry"));
					continue;
				}
			}
		}

		// Seed Source.Damage on the firing ASC. The ExecCalc captures
		// Source.Damage with bSnapshot=true at spec creation, so this write
		// MUST land before MakeOutgoingSpec. Override semantics overwrite any
		// stale value from a previous shot — the value is fully owned by this
		// ability per-shot. Mirrors UAFLGameplayAbility_DamageTest's seed step.
		SourceASC->ApplyModToAttribute(
			UAFLAttributeSet_Combat::GetDamageAttribute(),
			EGameplayModOp::Override,
			BaseDamage);

		// Build the spec with an effect context that carries the hit + the
		// claimed view origin. The struct's AddTargetDataToContext is invoked
		// by FGameplayAbilityTargetData_SingleTargetHit during context-add via
		// AddInstigator's default flow — we explicitly call it to be sure the
		// origin and hit make it onto the spec's context regardless of engine
		// version drift.
		FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
		ContextHandle.AddInstigator(CurrentActorInfo->OwnerActor.Get(), CurrentActorInfo->AvatarActor.Get());
		HitscanData->AddTargetDataToContext(ContextHandle, /*bIncludeActorArray=*/true);

		FGameplayEffectSpecHandle SpecHandle =
			SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), ContextHandle);
		if (!SpecHandle.IsValid())
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_PULSE: MakeOutgoingSpec(%s) returned invalid handle"),
				*GetNameSafe(DamageEffectClass));
			continue;
		}

		// Inject SetByCaller multipliers. ExecCalc reads these with default 1.0f
		// when absent — we always set them explicitly for predictable runs and
		// to establish the Headshot/Weakpoint/Distance hooks for AFL-0211/0213
		// tuning. Mirrors UAFLGameplayAbility_DamageTest::ActivateAbility step 3.
		FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Headshot_LaserPulse,  false), 1.0f);
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Weakpoint_LaserPulse, false), 1.0f);
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Distance_LaserPulse,  false), 1.0f);

		SourceASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC);
	}
}
#endif // WITH_SERVER_CODE

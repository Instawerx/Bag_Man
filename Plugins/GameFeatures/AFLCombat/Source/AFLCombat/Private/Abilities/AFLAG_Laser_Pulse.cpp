// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Laser_Pulse.h"

#include "AFLCombat.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Effects/GE_AFL_Damage_Pulse.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "Targeting/AFLAbilityTargetData_Hitscan.h"
#include "Telemetry/AFLCombatTelemetry.h"
#include "Tuning/AFLPulseTuningData.h"

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

	// Cooldown tag declared in AFLCombatTags.ini (Cooldown.Weapon.Pulse). The
	// concrete Cooldown GE is wired by the AbilitySet data asset in AFL-0214.
	// Cost GE is a placeholder until heat lands in AFL-0207; left unset here.

	// AFL-0105: default to the native pulse damage GE. BP children of the
	// ability can still override this on the CDO in AFL-0214 if they need a
	// designer-tuned variant.
	DamageEffectClass = UGE_AFL_Damage_Pulse::StaticClass();
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

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PULSE: Activate"));

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

	// Take ownership of the target data so downstream game code can't invalidate
	// it under us. Pattern lifted from ULyraGameplayAbility_RangedWeapon.
	FGameplayAbilityTargetDataHandle LocalTargetDataHandle(
		MoveTemp(const_cast<FGameplayAbilityTargetDataHandle&>(InData)));

	const bool bIsAuthority       = CurrentActorInfo->IsNetAuthority();
	const bool bIsLocallyControlled = CurrentActorInfo->IsLocallyControlled();

	// Client predicting on a remote client: ship the data to the server. On
	// the listen-server host both flags are true and we skip the RPC because
	// the server-side delegate fires from this same call.
	if (bIsLocallyControlled && !bIsAuthority)
	{
		FScopedPredictionWindow ScopedPrediction(ASC);
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
		// UAFLLagCompensationWorldSubsystem in AFL-0211. For now: log only,
		// still apply damage so the Sprint 1 PIE smoke test (AFL-0107) passes.
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

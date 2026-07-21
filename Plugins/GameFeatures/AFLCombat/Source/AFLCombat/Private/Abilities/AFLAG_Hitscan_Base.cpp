// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Hitscan_Base.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "NativeGameplayTags.h"
#include "Targeting/AFLAbilityTargetData_Hitscan.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_Hitscan_Base)

// Native tags -- module-load-safe (declared before any CDO ctor runs; a RequestGameplayTag in a ctor
// ensure-fails before the Tags/*.ini scan completes). Per-file _Hitscan suffix keeps the Unity build's
// merged translation unit from colliding these symbols with the laser abilities'.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Hitscan_HB,       "Ability.Hitscan");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Firing_Hitscan_HB,  "State.Firing.Hitscan");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_HB,        "State.Carrying");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_ThrowRecovery_HB,   "State.Weapon.ThrowRecovery");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Warmup_HB,    "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Ended_HB,     "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputTag_Weapon_Fire_HB,  "InputTag.Weapon.Fire");
// SetByCaller seed the proven UAFLDamageExecCalc reads (same contract Pulse/Charge document as Source.Damage).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Source_Damage_HB,         "Source.Damage");
// Cosmetic cues -- REUSE the proven generic laser cues (same strings Pulse/Charge fire), so the hitscan arsenal
// shares one FX contract. Fire+Tracer are burst; Charge is looping (Add on charge-start, Remove in EndAbility).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GC_Fire_HB,   "GameplayCue.Weapon.Pulse.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GC_Tracer_HB, "GameplayCue.Weapon.Pulse.Tracer");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GC_Charge_HB, "GameplayCue.Weapon.Laser.Charge");

UAFLAG_Hitscan_Base::UAFLAG_Hitscan_Base()
{
	// Same proven net contract as Pulse/Charge: owner predicts, InstancedPerActor for stable per-ASC state.
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// BOT-FIRE PARITY (mandatory): ShooterCore BTS_Shoot sends GameplayEvent(InputTag.Weapon.Fire). Without a
	// matching trigger bots can't fire (0 triggers). Separate from the player input path (AbilitySet InputTag).
	{
		FAbilityTriggerData FireEventTrigger;
		FireEventTrigger.TriggerTag    = TAG_InputTag_Weapon_Fire_HB;
		FireEventTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(FireEventTrigger);
	}

	AbilityTags.AddTag(TAG_Ability_Hitscan_HB);
	ActivationOwnedTags.AddTag(TAG_State_Firing_Hitscan_HB);
	ActivationBlockedTags.AddTag(TAG_State_Carrying_HB);
	ActivationBlockedTags.AddTag(TAG_State_ThrowRecovery_HB);
	ActivationBlockedTags.AddTag(TAG_State_Match_Warmup_HB);
	ActivationBlockedTags.AddTag(TAG_State_Match_Ended_HB);

	static ConstructorHelpers::FObjectFinder<UAnimMontage> FireMontageFinder(
		TEXT("/Game/Weapons/Rifle/Animations/AM_MM_Rifle_Fire.AM_MM_Rifle_Fire"));
	if (FireMontageFinder.Succeeded())
	{
		CharacterFireMontage = FireMontageFinder.Object;
	}
}

void UAFLAG_Hitscan_Base::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bFired = false;

	// Bind the target-data-ready callback (fires on the predicting client immediately, and on the server when
	// the client's replicated data arrives). Flush any already-cached data first (Lyra pattern).
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	OnTargetDataReadyCallbackDelegateHandle =
		ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
		   .AddUObject(this, &ThisClass::OnTargetDataReadyCallback);
	ASC->CallReplicatedTargetDataDelegatesIfSet(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	const bool bFromEvent = (TriggerEventData != nullptr); // GameplayEvent => bot path (no input to hold/release)

	if (!bChargeToFire)
	{
		// INSTANT (Pulse-style): fire immediately on activation, human and bot alike.
		Fire();
		return;
	}

	// CHARGE mode.
	if (bFromEvent)
	{
		// BOT PATH: no input to release. Synthesize a FULL charge and fire once (Charge's proven bot path).
		if (UWorld* World = GetWorld())
		{
			ChargeStartTimeSeconds = World->GetTimeSeconds() - static_cast<double>(MaxChargeTime);
		}
		Fire();
		return;
	}

	// HUMAN PATH: begin charging; fire on input release (if charged enough).
	if (UWorld* World = GetWorld())
	{
		ChargeStartTimeSeconds = World->GetTimeSeconds();
	}

	// Looping charge cue -- the build-up VFX/audio so the player SEES to HOLD (was the missing feedback that
	// made a charge weapon read as "doesn't fire"). SourceObject = the per-weapon tint provider. Removed in
	// EndAbility. Human path only (the bot fires instantly, nothing to wind up).
	{
		FGameplayCueParameters ChargeParams;
		ChargeParams.Instigator   = Cast<APawn>(GetAvatarActorFromActorInfo());
		UObject* Tint = ResolveLaserVisualProvider();
		ChargeParams.SourceObject = Tint ? Tint : static_cast<UObject*>(GetAvatarActorFromActorInfo());
		ASC->AddGameplayCue(TAG_GC_Charge_HB, ChargeParams);
		bChargeCueAdded = true;
	}

	WaitReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/false);
	if (WaitReleaseTask)
	{
		WaitReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnChargeInputReleased);
		WaitReleaseTask->ReadyForActivation();
	}
}

float UAFLAG_Hitscan_Base::ComputeChargeNorm() const
{
	const UWorld* World = GetWorld();
	if (!World || MaxChargeTime <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}
	const double Held = World->GetTimeSeconds() - ChargeStartTimeSeconds;
	return FMath::Clamp(static_cast<float>(Held) / MaxChargeTime, 0.0f, 1.0f);
}

void UAFLAG_Hitscan_Base::OnChargeInputReleased(float /*TimeHeld*/)
{
	if (ComputeChargeNorm() >= MinChargeToFire)
	{
		Fire();
	}
	else
	{
		// Undercharged: free cancel, no cost/cooldown (CommitAbility runs in Fire only).
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicate=*/true, /*bCancelled=*/true);
	}
}

void UAFLAG_Hitscan_Base::Fire()
{
	if (bFired)
	{
		return; // single shot per activation (charge-release + a stray call can't double-fire)
	}
	bFired = true;

	// Cooldown/cost gate (charge is free until the shot; the cooldown commits HERE, on fire only).
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Fire montage: fire-and-forget (single-shot EndAbility would blend an AndWait kick out). Owner-predicted +
	// authority; GAS dedups. Cosmetic only.
	if (CharacterFireMontage && CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		CurrentActorInfo->AbilitySystemComponent->PlayMontage(this, CurrentActivationInfo, CharacterFireMontage, 1.0f);
	}

	if (CurrentActorInfo && CurrentActorInfo->IsLocallyControlled())
	{
		ClientPredictAndSend();
	}
}

void UAFLAG_Hitscan_Base::PerformTrace(UWorld* World, const FVector& ViewOrigin, const FVector& AimDir,
	AActor* IgnoreActor, TArray<FHitResult>& OutHits) const
{
	OutHits.Reset();
	if (!World)
	{
		return;
	}
	const FVector End = ViewOrigin + AimDir * MaxRange;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLHitscan), /*bTraceComplex=*/true);
	Params.AddIgnoredActor(IgnoreActor);
	Params.bReturnPhysicalMaterial = true;

	if (bMultiHitPierce)
	{
		// PIERCE: every body along the ray, ordered near->far. The apply-loop damages each.
		World->LineTraceMultiByChannel(OutHits, ViewOrigin, End, TraceChannel, Params);
	}
	else
	{
		FHitResult Hit;
		World->LineTraceSingleByChannel(Hit, ViewOrigin, End, TraceChannel, Params);
		if (Hit.bBlockingHit)
		{
			OutHits.Add(Hit);
		}
	}
}

void UAFLAG_Hitscan_Base::ClientPredictAndSend()
{
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AvatarPawn || !ASC)
	{
		return;
	}

	// Camera-aligned origin + direction (PlayerCameraManager when present -- avatar view fallback for AI/bots).
	FVector  ViewLocation = AvatarPawn->GetPawnViewLocation();
	FRotator ViewRotation = AvatarPawn->GetViewRotation();
	if (APlayerController* PC = Cast<APlayerController>(AvatarPawn->GetController()))
	{
		if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
		{
			ViewLocation = CamMgr->GetCameraLocation();
			ViewRotation = CamMgr->GetCameraRotation();
		}
	}
	const FVector AimDir = ViewRotation.Vector().GetSafeNormal();

	TArray<FHitResult> Hits;
	PerformTrace(AvatarPawn->GetWorld(), ViewLocation, AimDir, AvatarPawn, Hits);

	// Pack EACH hit as its own FAFLAbilityTargetData_Hitscan into ONE handle. The server's apply-loop
	// iterates every entry -> pierce damages each body. Reuses the AFLNetTypes struct -> no GameFeature
	// net-struct registration trap. Ownership transfers to the handle (deletes on destruction).
	const FVector MuzzleLoc = ResolveMuzzleLocation(AvatarPawn);
	FGameplayAbilityTargetDataHandle Handle;
	for (const FHitResult& Hit : Hits)
	{
		FAFLAbilityTargetData_Hitscan* TD = new FAFLAbilityTargetData_Hitscan();
		TD->HitResult             = Hit;
		TD->ClaimedViewOrigin     = ViewLocation;
		TD->ClaimedAimDirection   = AimDir;
		TD->ClaimedMuzzleLocation = MuzzleLoc;
		Handle.Add(TD);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_HITSCAN: %s fired -> %d hit(s) (pierce=%d charge=%d)"),
		*GetNameSafe(AvatarPawn), Handle.Num(), bMultiHitPierce ? 1 : 0, bChargeToFire ? 1 : 0);

	FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());
	OnTargetDataReadyCallback(Handle, FGameplayTag());
}

void UAFLAG_Hitscan_Base::OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return;
	}
	const bool bIsLocallyControlled = CurrentActorInfo->IsLocallyControlled();
	const bool bIsAuthority         = CurrentActorInfo->IsNetAuthority();

	FGameplayAbilityTargetDataHandle LocalHandle = InData; // local copy; the incoming ref may be transient

	// COSMETICS (lifted from Pulse): muzzle flash ONCE at the barrel, then a Tracer per hit (muzzle->impact) so
	// pierce shows a beam to EACH pierced body. Fires on the predicting client AND proxies (shared prediction
	// window; GAS dedups the owner's predicted play against the authority multicast) so everyone sees the shot.
	// The muzzle world-point + hit ride the target data (ClaimedMuzzleLocation / HitResult) -- viewpoint-independent
	// (Beam_v2 model), so the tracer renders correctly from any camera incl. the proxy.
	if (APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
	{
		UObject* Tint = ResolveLaserVisualProvider();
		UObject* Src  = Tint ? Tint : static_cast<UObject*>(AvatarPawn);
		FVector MuzzleLoc = ResolveMuzzleLocation(AvatarPawn);
		if (LocalHandle.Num() > 0)
		{
			if (const FGameplayAbilityTargetData* R0 = LocalHandle.Get(0))
			{
				if (R0->GetScriptStruct() == FAFLAbilityTargetData_Hitscan::StaticStruct())
				{
					MuzzleLoc = FVector(static_cast<const FAFLAbilityTargetData_Hitscan*>(R0)->ClaimedMuzzleLocation);
				}
			}
		}
		FGameplayCueParameters FireParams;
		FireParams.Location = MuzzleLoc; FireParams.Instigator = AvatarPawn; FireParams.SourceObject = Src;
		K2_ExecuteGameplayCueWithParams(TAG_GC_Fire_HB, FireParams);

		for (int32 i = 0; i < LocalHandle.Num(); ++i)
		{
			const FGameplayAbilityTargetData* Raw = LocalHandle.Get(i);
			if (!Raw || Raw->GetScriptStruct() != FAFLAbilityTargetData_Hitscan::StaticStruct()) continue;
			const FAFLAbilityTargetData_Hitscan* TD = static_cast<const FAFLAbilityTargetData_Hitscan*>(Raw);
			FGameplayCueParameters TracerParams;
			TracerParams.Location = FVector(TD->ClaimedMuzzleLocation); TracerParams.Instigator = AvatarPawn; TracerParams.SourceObject = Src;
			FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
			Ctx.AddHitResult(TD->HitResult);
			TracerParams.EffectContext = Ctx;
			K2_ExecuteGameplayCueWithParams(TAG_GC_Tracer_HB, TracerParams);
		}
	}

	// Ship to server (remote predicting client only; on a listen-server host the server delegate fired inline).
	if (bIsLocallyControlled && !bIsAuthority)
	{
		ASC->CallServerSetReplicatedTargetData(
			CurrentSpecHandle,
			CurrentActivationInfo.GetActivationPredictionKey(),
			LocalHandle,
			ApplicationTag,
			ASC->ScopedPredictionKey);
	}

#if WITH_SERVER_CODE
	if (bIsAuthority)
	{
		ServerApplyTargetData(LocalHandle);
	}
#endif

	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	if (bIsLocallyControlled || bIsAuthority)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicate=*/true, /*bCancelled=*/false);
	}
}

#if WITH_SERVER_CODE
void UAFLAG_Hitscan_Base::ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data)
{
	UAbilitySystemComponent* SourceASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC || !DamageEffectClass)
	{
		return;
	}

	// LOOP every hit in the handle -> pierce damages each body. (Lifted from Pulse's Data.Num() loop, which was
	// already multi-capable.) LAG-COMP DEFERRED for the pilot: Pulse's per-hit rewind+ConfirmHit is the hardening
	// pass -- here we apply on the received hit directly to prove the pierce mechanic. Same loop is where the
	// lag-comp confirm slots in later (per hit).
	for (int32 Index = 0; Index < Data.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* Raw = Data.Get(Index);
		if (!Raw || Raw->GetScriptStruct() != FAFLAbilityTargetData_Hitscan::StaticStruct())
		{
			continue; // schema check: only our own payloads
		}
		const FAFLAbilityTargetData_Hitscan* TD = static_cast<const FAFLAbilityTargetData_Hitscan*>(Raw);

		AActor* HitActor = TD->HitResult.GetActor();
		UAbilitySystemComponent* TargetASC =
			HitActor ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor) : nullptr;
		if (!TargetASC)
		{
			continue;
		}

		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
		if (Spec.IsValid())
		{
			// Full-damage-through on pierce for the pilot (per-body falloff is a later scalar). UAFLDamageExecCalc
			// reads Source.Damage; a HitResult on the context routes zone/headshot in the ExecCalc.
			Spec.Data->SetSetByCallerMagnitude(TAG_Source_Damage_HB, BaseDamage);
			Spec.Data->GetContext().AddHitResult(TD->HitResult);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}
}
#endif // WITH_SERVER_CODE

void UAFLAG_Hitscan_Base::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Stop the looping charge cue (the one place the build-up VFX/audio actually ends -- on release-fire,
	// undercharged cancel, or any interrupt).
	if (bChargeCueAdded && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->RemoveGameplayCue(TAG_GC_Charge_HB);
		bChargeCueAdded = false;
	}

	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() && OnTargetDataReadyCallbackDelegateHandle.IsValid())
	{
		ActorInfo->AbilitySystemComponent->AbilityTargetDataSetDelegate(
			CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
			.Remove(OnTargetDataReadyCallbackDelegateHandle);
		OnTargetDataReadyCallbackDelegateHandle.Reset();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Laser_Charge.h"

#include "AFLCombat.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Curves/CurveFloat.h"
#include "Effects/GE_AFL_Charge_Cooldown.h"
#include "Effects/GE_AFL_Damage_Charge.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "LagComp/AFLLagCompensationWorldSubsystem.h"
#include "NativeGameplayTags.h"
#include "Targeting/AFLAbilityTargetData_Hitscan.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_Laser_Charge)

// Native tags -- CDO-ctor-safe (module init precedes CDO construction; the Pulse.cpp rationale). Per-file
// _Charge symbol suffix is Unity-build-safe (the FName VALUE stays the canonical tag string).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Laser_Charge, "Ability.Laser.Charge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Firing_Charge, "State.Firing.Charge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_Charge, "State.Carrying");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_ThrowRecovery_Charge, "State.Weapon.ThrowRecovery");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Warmup_Charge, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Ended_Charge, "State.Match.Ended");

// The looping charge cue (build-up VFX/audio/shake). Its notify (AAFLCueNotify_LaserCharge) registers for
// this tag; AddGameplayCue drives it, RemoveGameplayCue (EndAbility) stops it.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayCue_Weapon_Laser_Charge, "GameplayCue.Weapon.Laser.Charge");
// The fire flash + tracer reuse the proven Pulse cues (generic laser flash/tracer; the charge shot is a
// single hitscan). A distinct heavier tracer is a follow-up.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayCue_Weapon_Pulse_Fire_Charge, "GameplayCue.Weapon.Pulse.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayCue_Weapon_Pulse_Tracer_Charge, "GameplayCue.Weapon.Pulse.Tracer");

// Bot-fire GameplayEvent trigger. Bots (ShooterCore BTS_Shoot) send a FIXED "InputTag.Weapon.Fire"
// GameplayEvent for AI -- NOT the equipped weapon's input tag -- so the bot trigger must be "Fire" (Pulse's
// proven bot tag). This is SEPARATE from this held weapon's HUMAN input: the AbilitySet grants on
// "InputTag.Weapon.FireAuto" (bound in InputData_Hero -- the same held-fire tag the proven ShotgunBeam
// sibling uses) and stays UNCHANGED. Only the *trigger* was wrong: the earlier "FireAuto" trigger never
// matched the bot's fixed "Fire" event, so bots could not fire it. (The ShotgunBeam carries no bot trigger
// at all, so it is NOT a bot-fire model -- Pulse is.)
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputTag_Weapon_Fire_ChargeTrigger, "InputTag.Weapon.Fire");

namespace
{
	// SetByCaller magnitude tags consumed by UAFLDamageExecCalc (default 1.0f absent). Per-file suffix, Unity-safe.
	const FName NAME_Data_Damage_Headshot_LaserCharge  = TEXT("Data.Damage.Headshot");
	const FName NAME_Data_Damage_Weakpoint_LaserCharge = TEXT("Data.Damage.Weakpoint");
	const FName NAME_Data_Damage_Distance_LaserCharge  = TEXT("Data.Damage.Distance");
}


UAFLAG_Laser_Charge::UAFLAG_Laser_Charge()
{
	// LocalPredicted single-shot-on-release, instanced-per-actor -- the same net contract as Pulse/Beam
	// (client predicts + traces + ships TargetData; server validates + applies).
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// HELD lifecycle: WhileInputActive owns activate+sustain; a single UAbilityTask_WaitInputRelease
	// (ActivateAbility) signals the release -> the FIRE. This is BeamChannel_v2's PROVEN live pattern
	// (BeamChannel_v2.cpp:53,135-140); the old "thrash" was a pressed-trigger instant-loop, NOT this task.
	ActivationPolicy = ELyraAbilityActivationPolicy::WhileInputActive;

	// AI-FIRE PARITY (conform to Pulse): bots fire by sending InputTag.Weapon.Fire as a GameplayEvent (no
	// input). The GameplayEvent path is SEPARATE from the human input path (the AbilitySet's InputTag), so
	// there is no double-activation. A bot's activation is COMPLETED by the bot branch in ActivateAbility
	// (below) -- a charge weapon has no input-release for a bot to trigger, so it fires-on-activation.
	{
		FAbilityTriggerData FireEventTrigger;
		FireEventTrigger.TriggerTag    = TAG_InputTag_Weapon_Fire_ChargeTrigger;
		FireEventTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(FireEventTrigger);
	}

	// Identity + owned-while-charging tags.
	AbilityTags.AddTag(TAG_Ability_Laser_Charge);
	ActivationOwnedTags.AddTag(TAG_State_Firing_Charge);

	// Blocked-tag set (mandatory conform, mirrors Pulse/Beam): no charge while carrying, in throw-recovery,
	// or during match Warmup / PostGame.
	ActivationBlockedTags.AddTag(TAG_State_Carrying_Charge);
	ActivationBlockedTags.AddTag(TAG_State_ThrowRecovery_Charge);
	ActivationBlockedTags.AddTag(TAG_State_Match_Warmup_Charge);
	ActivationBlockedTags.AddTag(TAG_State_Match_Ended_Charge);

	// Cooldown: C++-wired (unlike Pulse, which defers to the AbilitySet) so the power weapon can never ship
	// without its gate. GE_AFL_Charge_Cooldown grants Cooldown.Weapon.Charge; committed on FIRE only (release
	// >= MinChargeToFire), so an undercharged release costs nothing.
	CooldownGameplayEffectClass = UGE_AFL_Charge_Cooldown::StaticClass();

	// Damage GE -- routes through the proven UAFLDamageExecCalc via Source.Damage (seeded charge-scaled).
	DamageEffectClass = UGE_AFL_Damage_Charge::StaticClass();

	// Character fire montage -- the trigger-pull/kick, played once on release. Defaulted to the rifle 2H
	// additive brace (Pulse/Beam shape); the Tempest BP child may override to a heavier montage.
	static ConstructorHelpers::FObjectFinder<UAnimMontage> FireMontageFinder(
		TEXT("/Game/Weapons/Rifle/Animations/AM_MM_Rifle_Fire.AM_MM_Rifle_Fire"));
	if (FireMontageFinder.Succeeded())
	{
		CharacterFireMontage = FireMontageFinder.Object;
	}
}

void UAFLAG_Laser_Charge::ActivateAbility(
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
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	// DIVERGENCE from Pulse/Beam: NO CommitAbility here. Charge is free; the cooldown commits on FIRE only
	// (OnChargeInputReleased, Norm >= MinChargeToFire), so an undercharged release is not punished.

	// Charge start clock (both sides -- LocalPredicted). ComputeChargeNorm reads it on release.
	const UWorld* World = GetWorld();
	ChargeStartTimeSeconds = World ? World->GetTimeSeconds() : 0.0;

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_CHARGE: Activate (begin charge) by %s (role=%d localctrl=%d)"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		GetAvatarActorFromActorInfo() ? static_cast<int32>(GetAvatarActorFromActorInfo()->GetLocalRole()) : 0,
		ActorInfo->IsLocallyControlled() ? 1 : 0);

	// Bind the target-data delegate on BOTH sides (Pulse pattern): the fire path (human release, or the
	// synthesized bot shot below) dispatches TargetData -> OnTargetDataReadyCallback plays the montage + fire
	// cues. Bound BEFORE either path so a bot's immediate fire has it ready.
	OnTargetDataReadyCallbackDelegateHandle =
		ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
		   .AddUObject(this, &ThisClass::OnTargetDataReadyCallback);

	// BOT PATH (GameplayEvent-triggered): a bot has no input to hold or release, so a WaitInputRelease would
	// never complete and the bot would charge forever. Conform to Pulse's fire-on-activation -- synthesize a
	// FULL charge (backdate the clock so ComputeChargeNorm()==1) and go straight to the fire path ONCE. The
	// bot avatar is locally-controlled on the server, so OnChargeInputReleased -> ClientPredictAndSend fires
	// server-side exactly as Pulse does; GE_AFL_Charge_Cooldown rate-limits BTS_Shoot's per-tick events to
	// the fire cadence. No charge cue (nothing to wind up); EndAbility skips cue removal (bChargeCueAdded=false).
	if (TriggerEventData != nullptr)
	{
		ChargeStartTimeSeconds -= static_cast<double>(MaxChargeTime); // now - MaxChargeTime -> full charge
		OnChargeInputReleased(0.0f);
		return;
	}

	// HUMAN PATH -- looping charge cue (build-up VFX/audio/shake). SourceObject = the per-weapon tint provider
	// (beam color contract) so the charge glow tints per weapon. RemoveGameplayCue in EndAbility stops it.
	{
		FGameplayCueParameters ChargeCueParams;
		UObject* const TintProvider = ResolveLaserVisualProvider();
		ChargeCueParams.SourceObject = TintProvider ? TintProvider : static_cast<UObject*>(GetAvatarActorFromActorInfo());
		ChargeCueParams.Instigator   = GetAvatarActorFromActorInfo();
		ASC->AddGameplayCue(TAG_GameplayCue_Weapon_Laser_Charge, ChargeCueParams);
		bChargeCueAdded = true;
	}

	// Listen for release -> fire-or-cancel. Single WaitInputRelease (BeamChannel_v2's proven pattern; the
	// Fire input is held, so OnRelease fires ONCE on real button-up, no thrash).
	if (UAbilityTask_WaitInputRelease* ReleaseTask =
			UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/false))
	{
		ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnChargeInputReleased);
		ReleaseTask->ReadyForActivation();
	}
}

void UAFLAG_Laser_Charge::OnChargeInputReleased(float /*TimeHeld*/)
{
	const float Norm = ComputeChargeNorm();

	// Undercharge -> cancel: no shot, NO cooldown (CommitAbility never ran).
	if (Norm < MinChargeToFire)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_CHARGE: release below min (norm=%.2f < %.2f) -> cancel"), Norm, MinChargeToFire);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// FIRE: commit the cooldown now (both sides -- predicted on the client, authoritative on the server).
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_CHARGE: FIRE (norm=%.2f mult=%.2f)"), Norm, EvalChargeMultiplier(Norm));

	if (CurrentActorInfo && CurrentActorInfo->IsLocallyControlled())
	{
		// Local (or listen-host): build the shot from the camera + dispatch. Non-local dedicated server
		// sits on the target-data delegate and applies when the client's shipped data arrives.
		ClientPredictAndSend();
	}
}

void UAFLAG_Laser_Charge::ClientPredictAndSend()
{
	check(CurrentActorInfo);
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn)
	{
		return;
	}
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	// Camera-aligned origin + direction (PlayerCameraManager = the post-modifier viewpoint; falls back to
	// the pawn view for AI). DIVERGENCE from Pulse: NO spread bloom -- a charge shot is precise (fires
	// straight down the aim ray). Charge scales DAMAGE, not accuracy.
	FVector  ViewLocation = AvatarPawn->GetPawnViewLocation();
	FRotator ViewRotation = AvatarPawn->GetViewRotation();
	APlayerController* PC = Cast<APlayerController>(AvatarPawn->GetController());
	if (PC && PC->PlayerCameraManager)
	{
		ViewLocation = PC->PlayerCameraManager->GetCameraLocation();
		ViewRotation = PC->PlayerCameraManager->GetCameraRotation();
	}
	const FVector AimDirection = ViewRotation.Vector().GetSafeNormal();
	const FVector EndTrace     = ViewLocation + AimDirection * MaxRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLChargeHitscan), /*bTraceComplex=*/true);
	Params.AddIgnoredActor(AvatarPawn);
	Params.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	UWorld* World = AvatarPawn->GetWorld();
	if (World)
	{
		World->LineTraceSingleByChannel(Hit, ViewLocation, EndTrace, TraceChannel, Params);
	}
	if (!Hit.bBlockingHit)
	{
		Hit.TraceStart  = ViewLocation;
		Hit.TraceEnd    = EndTrace;
		Hit.Location    = EndTrace;
		Hit.ImpactPoint = EndTrace;
	}

	// Pack the SAME hitscan payload Pulse/Beam use (schema the server + ExecCalc understand). No spread ->
	// ClaimedAimDirection == the true aim ray; angular velocity is N/A for a single charged shot (0).
	FAFLAbilityTargetData_Hitscan* NewTargetData = new FAFLAbilityTargetData_Hitscan();
	NewTargetData->HitResult                   = Hit;
	NewTargetData->ClaimedViewOrigin           = ViewLocation;
	NewTargetData->ClaimedAimDirection         = AimDirection;
	NewTargetData->AimAngularVelocityDegPerSec = 0.0f;
	NewTargetData->ClaimedMuzzleLocation       = ResolveMuzzleLocation(AvatarPawn);

	FGameplayAbilityTargetDataHandle TargetDataHandle;
	TargetDataHandle.Add(NewTargetData);

	// The predicted local callback runs under the activation prediction key the server will see.
	{
		FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());
		OnTargetDataReadyCallback(TargetDataHandle, FGameplayTag());
	}
}

void UAFLAG_Laser_Charge::OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag)
{
	check(CurrentActorInfo);
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	if (!ASC->FindAbilitySpecFromHandle(CurrentSpecHandle))
	{
		return; // cancelled between dispatch + delegate
	}

	// Shared prediction window (Pulse pattern): cosmetics + server-send under ONE key so GAS dedups the
	// owner's predicted play against the authority multicast (no double, no proxy mirror).
	FScopedPredictionWindow CueScopedPrediction(ASC);
	FGameplayAbilityTargetDataHandle LocalTargetDataHandle(
		MoveTemp(const_cast<FGameplayAbilityTargetDataHandle&>(InData)));

	// CharacterFireMontage -- fire-and-forget (NOT the AndWait task; single-shot EndAbility at the bottom of
	// this callback would blend the kick out). ClearAnimatingAbility (EndAbility) unlinks without stopping it.
	if (CharacterFireMontage)
	{
		ASC->PlayMontage(this, CurrentActivationInfo, CharacterFireMontage, 1.0f);
	}

	const bool bIsAuthority         = CurrentActorInfo->IsNetAuthority();
	const bool bIsLocallyControlled = CurrentActorInfo->IsLocallyControlled();

	// Fire flash + tracer cues (role-agnostic, inside the window; reuse the proven Pulse cues). Muzzle from
	// the client-authoritative payload; tracer end from the real Hit via the EffectContext.
	if (LocalTargetDataHandle.Num() > 0)
	{
		const FGameplayAbilityTargetData* RawData = LocalTargetDataHandle.Get(0);
		if (RawData && RawData->GetHitResult() &&
			RawData->GetScriptStruct() == FAFLAbilityTargetData_Hitscan::StaticStruct())
		{
			APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
			if (AvatarPawn)
			{
				UObject* const TintProvider = ResolveLaserVisualProvider();
				const FAFLAbilityTargetData_Hitscan* HitscanData =
					static_cast<const FAFLAbilityTargetData_Hitscan*>(RawData);
				const FHitResult& HitRef = *RawData->GetHitResult();

				{
					FGameplayCueParameters FireCueParams;
					FireCueParams.Location            = HitscanData->ClaimedMuzzleLocation;
					FireCueParams.Normal              = HitscanData->ClaimedAimDirection.GetSafeNormal();
					FireCueParams.Instigator          = AvatarPawn;
					FireCueParams.SourceObject         = TintProvider ? TintProvider : static_cast<UObject*>(AvatarPawn);
					FireCueParams.NormalizedMagnitude = ComputeChargeNorm(); // charge intensity for the flash
					K2_ExecuteGameplayCueWithParams(TAG_GameplayCue_Weapon_Pulse_Fire_Charge, FireCueParams);
				}
				{
					FGameplayCueParameters TracerParams;
					TracerParams.Location     = HitscanData->ClaimedMuzzleLocation;
					TracerParams.Instigator   = AvatarPawn;
					TracerParams.SourceObject = TintProvider ? TintProvider : static_cast<UObject*>(AvatarPawn);
					FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
					Ctx.AddHitResult(HitRef);
					TracerParams.EffectContext = Ctx;
					K2_ExecuteGameplayCueWithParams(TAG_GameplayCue_Weapon_Pulse_Tracer_Charge, TracerParams);
				}
			}
		}
	}

	// Remote client -> ship to server under the SAME window key (Pulse's one-window shape).
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

	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	// Single charged shot -> end here (both viewpoints), exactly like Pulse.
	if (bIsLocallyControlled || bIsAuthority)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			/*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
	}
}

#if WITH_SERVER_CODE
void UAFLAG_Laser_Charge::ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data)
{
	UAbilitySystemComponent* SourceASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC || !DamageEffectClass)
	{
		return;
	}

	// Charge multiplier from the SERVER's own charge timing (authoritative; capped at Norm=1). This is the
	// ONE gameplay divergence from Pulse: BaseDamage is scaled by ChargeCurve.Eval(Norm) at the seed.
	const float Norm         = ComputeChargeNorm();
	const float ChargeMult   = EvalChargeMultiplier(Norm);
	const float ScaledDamage = BaseDamage * ChargeMult;

	for (int32 Index = 0; Index < Data.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* RawData = Data.Get(Index);
		if (!RawData || RawData->GetScriptStruct() != FAFLAbilityTargetData_Hitscan::StaticStruct())
		{
			continue;
		}
		FAFLAbilityTargetData_Hitscan* HitscanData =
			const_cast<FAFLAbilityTargetData_Hitscan*>(static_cast<const FAFLAbilityTargetData_Hitscan*>(RawData));

		AActor* HitActor = HitscanData->HitResult.GetActor();
		if (!HitActor)
		{
			continue;
		}
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC)
		{
			continue;
		}

		// Lag-comp rewind + confirm (the shared shipping path; also resolves the zone bone for dismember).
		if (UWorld* World = GetWorld())
		{
			if (UAFLLagCompensationWorldSubsystem* LagComp = World->GetSubsystem<UAFLLagCompensationWorldSubsystem>())
			{
				APlayerController* SourcePC = CurrentActorInfo ? CurrentActorInfo->PlayerController.Get() : nullptr;
				const APlayerState* SourcePS = SourcePC ? SourcePC->PlayerState : nullptr;
				const float RawRTT     = SourcePS ? (SourcePS->GetPingInMilliseconds() * 0.001f) : 0.0f;
				const float ClampedRTT = FMath::Min(RawRTT, 0.2f);
				FName ResolvedBone = NAME_None;
				if (!LagComp->ConfirmHit(SourcePC, ClampedRTT, HitActor, HitscanData->HitResult.ImpactPoint, ResolvedBone))
				{
					UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_CHARGE: hitscan_reject reason=geometry"));
					continue;
				}
				if (!ResolvedBone.IsNone())
				{
					HitscanData->HitResult.BoneName = ResolvedBone;
				}
			}
		}

		// Seed the charge-scaled Source.Damage BEFORE MakeOutgoingSpec (ExecCalc snapshots it).
		SourceASC->ApplyModToAttribute(
			UAFLAttributeSet_Combat::GetDamageAttribute(),
			EGameplayModOp::Override,
			ScaledDamage);

		FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
		ContextHandle.AddInstigator(CurrentActorInfo->OwnerActor.Get(), CurrentActorInfo->AvatarActor.Get());
		HitscanData->AddTargetDataToContext(ContextHandle, /*bIncludeActorArray=*/true);

		FGameplayEffectSpecHandle SpecHandle =
			SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), ContextHandle);
		if (!SpecHandle.IsValid())
		{
			continue;
		}
		FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Headshot_LaserCharge,  false), 1.0f);
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Weakpoint_LaserCharge, false), 1.0f);
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Distance_LaserCharge,  false), 1.0f);

		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_CHARGE: applied dmg=%.1f (base=%.1f x mult=%.2f, norm=%.2f) to %s"),
			ScaledDamage, BaseDamage, ChargeMult, Norm, *GetNameSafe(HitActor));
	}
}
#endif

void UAFLAG_Laser_Charge::EndAbility(
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

			// The looping charge cue's OnRemove is the ONE place its VFX/audio/shake stop -- remove on BOTH
			// the fire-end and the undercharge-cancel path (this is that single teardown point).
			if (bChargeCueAdded)
			{
				ASC->RemoveGameplayCue(TAG_GameplayCue_Weapon_Laser_Charge);
				bChargeCueAdded = false;
			}

			ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
			   .Remove(OnTargetDataReadyCallbackDelegateHandle);
			ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
		}

		ChargeStartTimeSeconds = -1.0;

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

float UAFLAG_Laser_Charge::ComputeChargeNorm() const
{
	if (ChargeStartTimeSeconds < 0.0)
	{
		return 0.0f;
	}
	const UWorld* World = GetWorld();
	const double Now  = World ? World->GetTimeSeconds() : ChargeStartTimeSeconds;
	const double Held = FMath::Max(0.0, Now - ChargeStartTimeSeconds);
	return FMath::Clamp(static_cast<float>(Held / FMath::Max(0.05, static_cast<double>(MaxChargeTime))), 0.0f, 1.0f);
}

float UAFLAG_Laser_Charge::EvalChargeMultiplier(float Norm) const
{
	if (ChargeCurve)
	{
		return ChargeCurve->GetFloatValue(Norm);
	}
	// Null-safe fallback: linear 0..1 -> 0.5..1.0 (min charge = half damage, full = full).
	return FMath::Lerp(0.5f, 1.0f, FMath::Clamp(Norm, 0.0f, 1.0f));
}

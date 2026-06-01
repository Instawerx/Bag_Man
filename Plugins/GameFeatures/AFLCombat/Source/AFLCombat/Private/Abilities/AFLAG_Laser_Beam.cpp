// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Laser_Beam.h"

#include "AFLCombat.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Effects/GE_AFL_Cooldown_Beam.h"
#include "Effects/GE_AFL_Damage_BeamTick.h"
#include "Effects/GE_AFL_Heat_BeamTick.h"
#include "Effects/GE_AFL_Heat_CoolingGate.h"
#include "Effects/GE_AFL_Heat_Decay.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"
#include "Targeting/AFLAbilityTargetData_Hitscan.h"
#include "Telemetry/AFLCombatTelemetry.h"
// AFL-0208 (RP-2): Niagara / ConstructorHelpers / mesh-component includes removed --
// the beam VFX moved to the GameplayCue (AAFLCueNotify_LaserBeam). The ability only
// triggers the cue tag; it no longer spawns Niagara or resolves a muzzle socket.

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_Laser_Beam)

// Native tag declarations. Same module-load-vs-ini-scan rationale as
// AFLAG_Laser_Pulse — CDO construction runs before per-plugin Tags/*.ini
// scans complete, so FGameplayTag::RequestGameplayTag in the ctor would
// ensure-fail. UE_DEFINE_GAMEPLAY_TAG_STATIC registers the tag at module
// init, strictly before any CDO of a class in this module is constructed.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Laser_Beam, "Ability.Laser.Beam");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Firing_Beam, "State.Firing.Beam");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Overheated_Beam, "State.Overheated");
// AFL-0208 (RP-2): the looping beam cosmetic cue. Added on activate, removed on end.
// Defined in AFLCombatTags.ini; received by GCN_AFL_Laser_Beam (AAFLCueNotify_LaserBeam).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayCue_Weapon_Laser_Beam, "GameplayCue.Weapon.Laser.Beam");

// Telemetry reason category for AFL-0213 stable format. EmitRejection logs
// `AFL_TELEMETRY: hitscan_reject reason=beam_tick source=...` on rejected
// per-tick payloads (currently only schema mismatch — geometry / lag-comp
// rejects land with AFL-0211).
static const FName NAME_BeamTickReject = TEXT("beam_tick");


UAFLAG_Laser_Beam::UAFLAG_Laser_Beam()
{
	// Locally-predicted, instanced-per-actor — same shape as Pulse. The
	// per-tick TargetData payloads ride the same prediction key the
	// activation opened, so all the ticks for a single channel batch into
	// one client-prediction window.
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// AFL-0208 (channel fix): hold-to-channel. The default ELyraAbilityActivationPolicy is
	// OnInputTriggered (fire-once-on-press) -- correct for the hitscan Pulse, but WRONG for
	// a channeled beam: under OnInputTriggered the input-pressed state is consumed on
	// activation, so UAbilityTask_WaitInputRelease fires IMMEDIATELY (Activate->Release same
	// frame) and the beam never sustains -- it flashes for one frame. WhileInputActive makes
	// LyraASC keep the ability active while LMB is held (LyraAbilitySystemComponent input
	// handler) and only release on real button-up. This is THE fix for the instant-release.
	ActivationPolicy = ELyraAbilityActivationPolicy::WhileInputActive;

	AbilityTags.AddTag(TAG_Ability_Laser_Beam);
	ActivationOwnedTags.AddTag(TAG_State_Firing_Beam);

	// Block activation while State.Overheated is set on the source. AFL-0207
	// adds Overheated as a loose replicated tag from
	// UAFLAttributeSet_Combat::PostGameplayEffectExecute when Heat reaches
	// MaxHeat; until Heat decays below MaxHeat*0.3 the beam cannot re-channel.
	ActivationBlockedTags.AddTag(TAG_State_Overheated_Beam);

	// Defaults for the GEs we drive. BP children can override these on the
	// CDO once AFL-0214 introduces designer-tuned variants.
	DamageEffectClass          = UGE_AFL_Damage_BeamTick::StaticClass();
	ReleaseCooldownEffectClass = UGE_AFL_Cooldown_Beam::StaticClass();
	HeatTickEffectClass        = UGE_AFL_Heat_BeamTick::StaticClass();
	HeatCoolingGateEffectClass = UGE_AFL_Heat_CoolingGate::StaticClass();
	HeatDecayEffectClass       = UGE_AFL_Heat_Decay::StaticClass();

	// AFL-0208 (RP-2): no beam-VFX FObjectFinders here anymore. The beam system + impact
	// spark are owned by the GameplayCue (AAFLCueNotify_LaserBeam) and the weapon's
	// DA_AFL_LaserVisual, not the ability CDO. The ability only triggers the cue tag.
}

void UAFLAG_Laser_Beam::ActivateAbility(
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
		// Cost/cooldown gate (Cooldown.Weapon.Beam blocks re-channel, heat
		// will block once AFL-0207 wires it). CommitAbility cancels the
		// prediction key for us; just bail.
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BEAM: Activate"));

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	// Authority ensures Heat_Decay is present on the source ASC. Idempotent:
	// if a Decay GE is already active we skip. Once AFL-0214's AbilitySet
	// grants Decay at pawn spawn this block becomes redundant but harmless.
	if (ActorInfo->IsNetAuthority() && HeatDecayEffectClass)
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = HeatDecayEffectClass;
		if (ASC->GetActiveEffects(Query).Num() == 0)
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddInstigator(ActorInfo->OwnerActor.Get(), ActorInfo->AvatarActor.Get());
			FGameplayEffectSpecHandle SpecHandle =
				ASC->MakeOutgoingSpec(HeatDecayEffectClass, GetAbilityLevel(), Context);
			if (SpecHandle.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
	}

	// Bind the target-data delegate on BOTH sides. Same pattern as Pulse:
	//   * Client: fires immediately each tick from OnTargetDataReadyCallback.
	//   * Server: fires when the replicated per-tick TargetData arrives.
	// EndAbility unbinds via the saved handle. Beam channels re-use the same
	// CurrentSpecHandle + activation prediction key for every tick, so a
	// single bind covers the whole channel.
	OnTargetDataReadyCallbackDelegateHandle =
		ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
		   .AddUObject(this, &ThisClass::OnTargetDataReadyCallback);

	// Listen for input release on both client and server. The standard GAS
	// task replicates the release event up from the client; the server runs
	// its own copy of the task in parallel and fires the delegate when the
	// replicated event lands.
	if (UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(
			this, /*bTestAlreadyReleased=*/false))
	{
		ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
		ReleaseTask->ReadyForActivation();
	}

	if (ActorInfo->IsLocallyControlled())
	{
		// Local predicting client (or listen-server host): start the 100ms
		// tick timer. Dedicated-server sim proxies don't trace — they sit
		// on the target-data delegate and wait for the replicated payloads.
		if (UWorld* World = ActorInfo->AvatarActor.IsValid()
				? ActorInfo->AvatarActor->GetWorld()
				: nullptr)
		{
			// AFL-0208 (RP-2): the sustained beam VFX is now a GameplayCue, not an
			// ability-owned Niagara spawn. AddGameplayCue(GameplayCue.Weapon.Laser.Beam)
			// hands the cosmetic to AAFLCueNotify_LaserBeam (AFLVFX, always-on plugin),
			// which spawns the imported beam system, anchors its start on the aim ray
			// (the verified BeamVisualOriginDistance logic, ported into the cue), and
			// drives User.Beam End to its own cosmetic trace's impact. SourceObject is
			// the weapon instance (implements IAFLLaserVisualProvider -> beam system +
			// color). RemoveGameplayCue in EndAbility tears it down (OnRemove). The cue
			// replicates + is net-decoupled for free; no Niagara code lives here anymore.
			// Cosmetic only -> inside the IsLocallyControlled gate (the cue add predicts
			// on the firing client and replicates).
			{
				FGameplayCueParameters BeamCueParams;
				BeamCueParams.SourceObject = GetAFLLaserWeaponInstance();
				BeamCueParams.Instigator   = GetAvatarActorFromActorInfo();
				ASC->AddGameplayCue(TAG_GameplayCue_Weapon_Laser_Beam, BeamCueParams);
			}

			// Fire the first tick immediately so the channel produces a
			// hitmarker on frame 0 of the hold; the timer then settles into
			// its TickInterval cadence for the rest of the channel.
			TickChannel();

			World->GetTimerManager().SetTimer(
				TickTimerHandle,
				FTimerDelegate::CreateUObject(this, &ThisClass::TickChannel),
				TickInterval,
				/*InbLoop=*/true);
		}
	}
}

void UAFLAG_Laser_Beam::EndAbility(
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

		// Stop the tick timer regardless of which side we're on. Idempotent
		// when the timer was never set (dedicated server avatar).
		if (TickTimerHandle.IsValid() && ActorInfo && ActorInfo->AvatarActor.IsValid())
		{
			if (UWorld* World = ActorInfo->AvatarActor->GetWorld())
			{
				World->GetTimerManager().ClearTimer(TickTimerHandle);
			}
		}

		// AFL-0208 (RP-2): tear down the beam cosmetic cue on channel end (release,
		// overheat self-cancel, or any cancel). RemoveGameplayCue fires the cue's
		// OnRemove, which deactivates + auto-destroys the Niagara. Mirrors the
		// AddGameplayCue in ActivateAbility; idempotent if it was never added.
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

			ASC->RemoveGameplayCue(TAG_GameplayCue_Weapon_Laser_Beam);

			ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
			   .Remove(OnTargetDataReadyCallbackDelegateHandle);
			ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
		}

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

void UAFLAG_Laser_Beam::TickChannel()
{
	check(CurrentActorInfo);

	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn)
	{
		return;
	}

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	// Camera-aligned origin and direction via PlayerCameraManager. Same
	// rationale as Pulse: AFL-0215 lint rejects the controller's view-helper
	// anywhere in AFLCombat, so we read the camera manager surface (which
	// exposes the same post-modifier viewpoint) and fall back to the pawn's
	// view helpers for AI / split-screen edge cases.
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

	const FVector AimDirection = ViewRotation.Vector().GetSafeNormal();
	const FVector EndTrace     = ViewLocation + AimDirection * MaxRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLBeamHitscan), /*bTraceComplex=*/true);
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

	// AFL-0208 (RP-2): the per-tick beam/impact VFX drive lived here (Path B --
	// SetVariableVec3 BeamStart/BeamEnd + the impact-spark array feed). It moved into
	// AAFLCueNotify_LaserBeam, which derives the SAME aim-ray endpoint itself each tick
	// (the BeamVisualOriginDistance + point-blank-clamp math was ported verbatim into
	// the cue). The ability no longer touches Niagara -- it only computes the
	// authoritative Hit (above) for the target-data payload (below).

	// Reuse the Pulse hitscan struct — the brief is explicit that Beam does
	// NOT fork a new target-data type. The server applies a different GE
	// per tick (BeamTick vs Pulse), but the payload schema is identical.
	FAFLAbilityTargetData_Hitscan* NewTargetData = new FAFLAbilityTargetData_Hitscan();
	NewTargetData->HitResult                   = Hit;
	NewTargetData->ClaimedViewOrigin           = ViewLocation;
	NewTargetData->ClaimedAimDirection         = AimDirection;
	NewTargetData->AimAngularVelocityDegPerSec = 0.0f;

	FGameplayAbilityTargetDataHandle TargetDataHandle;
	TargetDataHandle.Add(NewTargetData);

	// Open a per-tick prediction window inside the ability's activation key
	// so the OnTargetDataReadyCallback dispatch is in-key. The CallServerSetReplicatedTargetData
	// inside the callback uses ASC->ScopedPredictionKey which we set here.
	{
		FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());
		OnTargetDataReadyCallback(TargetDataHandle, FGameplayTag());
	}
}

void UAFLAG_Laser_Beam::OnInputReleased(float /*TimeHeld*/)
{
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BEAM: Release"));

	// Apply the cooldown source-side. The WaitInputRelease task runs in
	// parallel on client and server (the engine replicates the input event
	// up), so this fires on both sides — the cooldown GE only takes effect
	// on the authority but a client-side apply is a no-op there.
	ApplyReleaseCooldown();

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		/*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UAFLAG_Laser_Beam::OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag)
{
	check(CurrentActorInfo);
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	if (!ASC->FindAbilitySpecFromHandle(CurrentSpecHandle))
	{
		// Ability cancelled out from under us between dispatch and delegate.
		return;
	}

	FGameplayAbilityTargetDataHandle LocalTargetDataHandle(
		MoveTemp(const_cast<FGameplayAbilityTargetDataHandle&>(InData)));

	const bool bIsAuthority         = CurrentActorInfo->IsNetAuthority();
	const bool bIsLocallyControlled = CurrentActorInfo->IsLocallyControlled();

	// Client predicting on a remote client: ship the per-tick payload up.
	// Listen-server host: both flags true, skip the RPC because the server
	// delegate fires from this same call.
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
	// consumed it. Safe to call on the client too. Note that we do NOT
	// EndAbility from here — unlike Pulse, Beam keeps running until the
	// input release task fires. The next tick produces the next payload.
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
}

#if WITH_SERVER_CODE
void UAFLAG_Laser_Beam::ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data)
{
	UAbilitySystemComponent* SourceASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC || !DamageEffectClass)
	{
		return;
	}

	// AFL-0207 per-tick heat: apply BeamTick (+4 HeatPerBeamTick) and
	// CoolingGate (0.5s gate that suppresses Heat_Decay) source-to-source on
	// every authoritative tick, regardless of whether the trace hit anything.
	// CoolingGate is removed first so re-application produces a fresh 0.5s
	// window — DurationPolicy alone doesn't refresh existing actives.
	if (HeatCoolingGateEffectClass)
	{
		FGameplayEffectQuery RemoveQuery;
		RemoveQuery.EffectDefinition = HeatCoolingGateEffectClass;
		SourceASC->RemoveActiveEffects(RemoveQuery);

		FGameplayEffectContextHandle GateContext = SourceASC->MakeEffectContext();
		GateContext.AddInstigator(CurrentActorInfo->OwnerActor.Get(), CurrentActorInfo->AvatarActor.Get());
		FGameplayEffectSpecHandle GateSpec =
			SourceASC->MakeOutgoingSpec(HeatCoolingGateEffectClass, GetAbilityLevel(), GateContext);
		if (GateSpec.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToSelf(*GateSpec.Data.Get());
		}
	}

	if (HeatTickEffectClass)
	{
		FGameplayEffectContextHandle HeatContext = SourceASC->MakeEffectContext();
		HeatContext.AddInstigator(CurrentActorInfo->OwnerActor.Get(), CurrentActorInfo->AvatarActor.Get());
		FGameplayEffectSpecHandle HeatSpec =
			SourceASC->MakeOutgoingSpec(HeatTickEffectClass, GetAbilityLevel(), HeatContext);
		if (HeatSpec.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToSelf(*HeatSpec.Data.Get());
		}
	}

	// Mid-channel overheat check. The heat-tick we just applied may have
	// driven Heat to MaxHeat and granted State.Overheated; if so, end the
	// channel before the damage GEs run so the player can't squeeze an extra
	// damage tick out of the overheat boundary. ActivationBlockedTags only
	// gates re-entry — once an ability is already active the engine doesn't
	// re-check those tags.
	if (SourceASC->HasMatchingGameplayTag(TAG_State_Overheated_Beam))
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BEAM: overheat — ending channel"));
		// Apply the standard release cooldown so the player can't immediately
		// re-channel once the venting clears (and gets the normal cooldown
		// audio/HUD cue rather than a special overheat-end one — that's S5
		// tuning, AFL-0204b).
		ApplyReleaseCooldown();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			/*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	for (int32 Index = 0; Index < Data.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* RawData = Data.Get(Index);
		if (!RawData)
		{
			continue;
		}

		// Schema reject — mirror the Pulse path. AFL-0213 stable format with
		// reason=beam_tick so log scrapers can distinguish per-weapon
		// rejections without parsing the source name.
		if (RawData->GetScriptStruct() != FAFLAbilityTargetData_Hitscan::StaticStruct())
		{
			const UScriptStruct* ActualStruct = RawData->GetScriptStruct();
			FAFLCombatTelemetry::EmitRejection(
				NAME_BeamTickReject,
				CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr,
				FString::Printf(TEXT("struct=%s"),
					ActualStruct ? *ActualStruct->GetName() : TEXT("null")));
			continue;
		}

		const FAFLAbilityTargetData_Hitscan* HitscanData = static_cast<const FAFLAbilityTargetData_Hitscan*>(RawData);

		AActor* HitActor = HitscanData->HitResult.GetActor();
		if (!HitActor)
		{
			// Whiff tick — no hit, no damage. We don't emit a reject for this
			// because misses are expected; only telemetry-worthy reject
			// categories are AFL-0213 stable-format events.
			continue;
		}

		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC)
		{
			continue;
		}

		FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
		ContextHandle.AddInstigator(CurrentActorInfo->OwnerActor.Get(), CurrentActorInfo->AvatarActor.Get());
		HitscanData->AddTargetDataToContext(ContextHandle, /*bIncludeActorArray=*/true);

		FGameplayEffectSpecHandle SpecHandle =
			SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), ContextHandle);
		if (!SpecHandle.IsValid())
		{
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_BEAM: MakeOutgoingSpec(%s) returned invalid handle"),
				*GetNameSafe(DamageEffectClass));
			continue;
		}

		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

		// Successful tick log per spec ("AFL_LOG: beam_tick damage=1.2"). The
		// magnitude is sourced from the GE's first Damage modifier so BP
		// children that tune it stay in sync with the log. Falls back to
		// 1.2 if the GE has been edited to a non-standard shape.
		float DamageMagnitude = 1.2f;
		if (const UGameplayEffect* GECdo = DamageEffectClass.GetDefaultObject())
		{
			if (GECdo->Modifiers.Num() > 0)
			{
				GECdo->Modifiers[0].ModifierMagnitude.GetStaticMagnitudeIfPossible(
					GetAbilityLevel(), DamageMagnitude);
			}
		}
		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_LOG: beam_tick damage=%.2f target=%s"),
			DamageMagnitude,
			*GetNameSafe(HitActor));
	}
}
#endif // WITH_SERVER_CODE

void UAFLAG_Laser_Beam::ApplyReleaseCooldown()
{
	if (!ReleaseCooldownEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return;
	}

	// Authority-only — cooldowns are server-authoritative and replicate down
	// via the GE's tag container. A client-side apply would create a desync.
	if (!CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(CurrentActorInfo->OwnerActor.Get(), CurrentActorInfo->AvatarActor.Get());

	FGameplayEffectSpecHandle SpecHandle =
		ASC->MakeOutgoingSpec(ReleaseCooldownEffectClass, GetAbilityLevel(), Context);
	if (SpecHandle.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

UObject* UAFLAG_Laser_Beam::GetAFLLaserWeaponInstance() const
{
	// AFL-0208 (RP-2): the weapon/equipment instance that granted this ability and
	// implements IAFLLaserVisualProvider (the beam look). The WID AbilitySet grant
	// (AbilitySet_AFL_BeamFire) sets the ability spec's SourceObject to the
	// ULyraEquipmentInstance, so this mirrors
	// ULyraGameplayAbility_FromEquipment::GetAssociatedEquipment without reparenting:
	// read the current spec's SourceObject directly. Returned as the beam cue's
	// SourceObject; the cue casts it to IAFLLaserVisualProvider.
	if (FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		return Spec->SourceObject.Get();
	}
	return nullptr;
}

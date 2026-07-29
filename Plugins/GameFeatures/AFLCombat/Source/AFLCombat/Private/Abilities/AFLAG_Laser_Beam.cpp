// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Laser_Beam.h"

#include "AFLCombat.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Beam/AFLBeamChannelComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Effects/GE_AFL_Cooldown_Beam.h"
#include "Effects/GE_AFL_Damage_BeamTick.h"
#include "Effects/GE_AFL_Heat_BeamTick.h"
#include "Effects/GE_AFL_Heat_CoolingGate.h"
#include "Effects/GE_AFL_Heat_Decay.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Equipment/LyraEquipmentInstance.h"   // side-scoped resolver: GetSpawnedActors()
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
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_LaserBeam, "State.Carrying");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_ThrowRecovery_LaserBeam, "State.Weapon.ThrowRecovery");
// Match-flow gates (S9 AFL-0902): no beam fire during Warmup or PostGame (mirror of Pulse + movement).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Warmup_LaserBeam, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Ended_LaserBeam, "State.Match.Ended");
// AFL-0208 (RP-2): the looping beam cosmetic cue. Added on activate, removed on end.
// Defined in AFLCombatTags.ini; received by GCN_AFL_Laser_Beam (AAFLCueNotify_LaserBeam).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GameplayCue_Weapon_Laser_Beam, "GameplayCue.Weapon.Laser.Beam");

// Telemetry reason category for AFL-0213 stable format. EmitRejection logs
// `AFL_TELEMETRY: hitscan_reject reason=beam_tick source=...` on rejected
// per-tick payloads (currently only schema mismatch — geometry / lag-comp
// rejects land with AFL-0211).
static const FName NAME_BeamTickReject = TEXT("beam_tick");

// SetByCaller magnitude tags consumed by UAFLDamageExecCalc (same tags as Pulse; the ExecCalc
// reads them with default 1.0f). Beam seeds these alongside Source.Damage so the ExecCalc has a
// non-zero base to compute the health delta from -- WITHOUT this seed the ExecCalc captures
// Source.Damage=0 and the tick is fully mitigated (no Health change), which is the
// "beam logs damage but dummy never dies" bug (BM-0103). Mirrors AFLAG_Laser_Pulse.
static const FName NAME_Data_Damage_Headshot_Beam  = TEXT("Data.Damage.Headshot");
static const FName NAME_Data_Damage_Weakpoint_Beam = TEXT("Data.Damage.Weakpoint");
static const FName NAME_Data_Damage_Distance_Beam  = TEXT("Data.Damage.Distance");


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

	// Throw cycle: no firing while carrying (the holstered rifle is hidden, not unequipped; LMB belongs
	// to the throw ability under State.Carrying -- same arbitration as Pulse).
	ActivationBlockedTags.AddTag(TAG_State_Carrying_LaserBeam);

	// ...and no channeling from the press/hold that THREW: GE_AFL_ThrowRecovery's 0.4s tag window. A
	// WhileInputActive ability re-tries every held frame, so it needs the time-based gate, not a
	// frame-based one (the PIE-caught beam-after-throw).
	ActivationBlockedTags.AddTag(TAG_State_ThrowRecovery_LaserBeam);

	// Match-flow freeze (S9 AFL-0902): no beam during Warmup or PostGame.
	ActivationBlockedTags.AddTag(TAG_State_Match_Warmup_LaserBeam);
	ActivationBlockedTags.AddTag(TAG_State_Match_Ended_LaserBeam);

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

	// PER-INSTANCE HEAT: commit the idle decay accrued since the last channel and clear the vent lock.
	// CanActivateAbility has already refused this activation unless HeatNorm is back at/below
	// HeatVentResumeNorm, so reaching here means this cannon is genuinely vented. Committing rather
	// than resetting keeps cooling honest -- a short pause resumes a hot beam, it doesn't hand back a
	// full gauge. Touches ONLY this instance; the other arm's heat is a different object.
	HeatNorm            = CurrentHeatNorm();
	LastHeatTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bOverheated         = false;

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

	// NO explicit WaitInputRelease task. ELyraAbilityActivationPolicy::WhileInputActive ALREADY
	// owns the held lifecycle: Lyra's ProcessAbilityInput activates this once while the input is
	// held, and CancelInputActivatedAbilities cancels it (-> EndAbility) on input-release. An
	// explicit WaitInputRelease here was REDUNDANT and actively harmful: on a trigger-less held
	// input (IA_Weapon_Fire_Auto fires Triggered every frame), the task fired OnRelease mid-hold ->
	// EndAbility -> the ability went !IsActive() -> ProcessAbilityInput re-activated it next frame
	// (input still held) -> a ~6x/sec Activate->Release THRASH (re-running damage/heat/cost/lag-comp
	// per cycle, and spawning a fresh beam cue each time = the "stuck"/stacked visual). Removing it
	// lets WhileInputActive run ONE continuous channel for the whole hold (canonical Lyra hold
	// ability). The release-cooldown moves to EndAbility (fires whenever Lyra cancels the channel).

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
				// AFL-0208 (published-value): open the beam-channel bridge BEFORE the cue is
				// added, so when the cue's OnActive runs it finds the component already
				// present + active. The ability publishes Hit.ImpactPoint into this each tick;
				// the cue reads it to drive User."Beam End". No Niagara crosses this boundary.
				if (UAFLBeamChannelComponent* Channel = ResolveBeamChannel())
				{
					Channel->SetBeamActive(true);
				}

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

		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BEAM: Release"));

		// Release-cooldown applies HERE (moved from the removed OnInputReleased): EndAbility is the
		// single channel-end point — fires whether Lyra's CancelInputActivatedAbilities cancelled the
		// held channel on input-release, or it self-cancelled (overheat). Authority-gated inside.
		ApplyReleaseCooldown();

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

		// AFL-0208 (published-value): close the beam-channel bridge. The cue's own
		// OnRemove (fired by RemoveGameplayCue above) destroys the Niagara; clearing
		// bBeamActive here is the authoritative "beam is off" signal that replicates,
		// and resetting the per-activation cache avoids a stale weak-ptr next channel.
		if (UAFLBeamChannelComponent* Channel = BeamChannel.Get())
		{
			Channel->SetBeamActive(false);
		}
		BeamChannel.Reset();

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

	// AFL-0208 (published-value): publish the authoritative endpoint into the beam-channel
	// bridge on the LOCALLY-CONTROLLED side -- immediate, so the firing client's cue beam
	// tracks with zero latency (no wait for a server round-trip). The authority publishes
	// the same value from ServerApplyTargetData so simulated proxies (other clients) get it
	// via replication. The ability touches NO Niagara; it only computes the authoritative
	// Hit (above) and hands the endpoint to the cosmetic layer through this one value.
	if (UAFLBeamChannelComponent* Channel = ResolveBeamChannel())
	{
		Channel->PublishImpact(Hit.ImpactPoint);
		// Operator precision rule: the visible beam emits from the barrel tip, not a synthetic
		// eye-point. Publish the muzzle world-location (Pulse's proven resolve + weapon_r
		// fallback) so the cue anchors the START there. Local-side immediate; the authority
		// path below publishes it too for sim proxies.
		Channel->PublishMuzzle(ResolveMuzzleLocation(GetAFLLaserWeaponInstance(), AvatarPawn));
	}

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

UAFLBeamChannelComponent* UAFLAG_Laser_Beam::ResolveBeamChannel()
{
	if (UAFLBeamChannelComponent* Cached = BeamChannel.Get())
	{
		return Cached;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return nullptr;
	}

	UAFLBeamChannelComponent* Channel = Avatar->FindComponentByClass<UAFLBeamChannelComponent>();
	if (!Channel && Avatar->HasAuthority())
	{
		// Create ONLY on the authority -- a server-registered replicated component
		// (stable name + SetIsReplicatedByDefault) propagates down to every client,
		// INCLUDING simulated proxies (the other-player case the 2-client gate checks).
		// Creating on a client too would race the replicated copy and produce a duplicate
		// (engine warns / skips non-stable-named runtime components for replication), so we
		// never create off the authority -- the autonomous proxy finds the replicated copy
		// (and writes its own immediate local value into it via TickChannel once present).
		// Idempotent ensure shape mirrors the Heat_Decay GE ensure; no content grant needed.
		Channel = NewObject<UAFLBeamChannelComponent>(Avatar, TEXT("AFLBeamChannel"));
		Channel->RegisterComponent();
	}

	// On a non-authority client before the replicated component has arrived, Channel is
	// null this frame; the caller no-ops (the cue seeds at the camera and corrects next
	// frame once replication delivers the component). Cache only a real component.
	if (Channel)
	{
		BeamChannel = Channel;
	}
	return Channel;
}

float UAFLAG_Laser_Beam::CurrentHeatNorm() const
{
	// Idle decay applied but NOT committed, so this is safe to call from the const CanActivateAbility.
	const UWorld* World = GetWorld();
	if (!World || LastHeatTimeSeconds <= 0.0)
	{
		return HeatNorm;
	}

	const float Gap = static_cast<float>(World->GetTimeSeconds() - LastHeatTimeSeconds);

	// Only the portion of the gap PAST the grace window cools -- the per-instance port of
	// GE_AFL_Heat_CoolingGate. A channel ticking every TickInterval (0.1s) never leaves the 0.5s
	// window, so it sheds nothing and heats at the full HeatPerTick, exactly as the gated GE did.
	const float IdleGap = FMath::Max(0.0f, Gap - HeatCoolingGraceSeconds);
	return FMath::Clamp(HeatNorm - HeatDecayPerSec * IdleGap, 0.0f, 1.0f);
}

float UAFLAG_Laser_Beam::AdvanceHeat()
{
	const UWorld* World = GetWorld();
	HeatNorm            = FMath::Clamp(CurrentHeatNorm() + HeatPerTick, 0.0f, 1.0f);
	LastHeatTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	return HeatNorm;
}

bool UAFLAG_Laser_Beam::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// PER-INSTANCE VENT GATE. Once this cannon overheats it stays locked until ITS OWN heat has cooled
	// to HeatVentResumeNorm -- the hysteresis the AttributeSet used to provide by holding State.Overheated
	// until Heat fell below MaxHeat * 0.3. Because the state is per ability instance, the other cannon's
	// gate is completely independent: one arm venting never blocks the other.
	if (bOverheated && CurrentHeatNorm() > HeatVentResumeNorm)
	{
		return false;
	}

	return true;
}

FVector UAFLAG_Laser_Beam::ResolveMuzzleLocation(UObject* SourceEquipment, APawn* AvatarPawn) const
{
	// SIDE-SCOPED overload -- the dual-mount (arm-cannon) correct resolve, mirroring
	// UAFLAG_Laser_Base's. ⚠ This class is a SIBLING of UAFLAG_Laser_Base, not a child
	// (UAFLAG_Laser_Beam : public ULyraGameplayAbility), so it cannot inherit that overload and
	// carries its own here -- same contract, its own equipment accessor (GetAFLLaserWeaponInstance).
	//
	// The pawn-scoped sibling below returns the FIRST "Muzzle"-socketed mesh across EVERY attached
	// actor: right for one held weapon, wrong for two (both cannons resolved the same first match).
	// Confining the search to the actors THIS equipment instance spawned gives each hand its own muzzle.
	if (const ULyraEquipmentInstance* Equipment = Cast<ULyraEquipmentInstance>(SourceEquipment))
	{
		for (AActor* Spawned : Equipment->GetSpawnedActors())
		{
			if (!IsValid(Spawned))
			{
				continue;
			}

			// UMeshComponent (not the old UStaticMeshComponent-only walk) so skeletal weapon meshes --
			// which every AFL harvest weapon uses -- resolve too.
			TInlineComponentArray<UMeshComponent*> MeshComps;
			Spawned->GetComponents<UMeshComponent>(MeshComps);
			for (UMeshComponent* MeshComp : MeshComps)
			{
				if (MeshComp && MeshComp->DoesSocketExist(FName("Muzzle")))
				{
					const FVector Resolved = MeshComp->GetSocketLocation(FName("Muzzle"));
					UE_LOG(LogAFLCombat, Verbose,
						TEXT("AFL_LASER/MUZZLE(side): 'Muzzle' on %s (equip %s) at world=%s"),
						*MeshComp->GetName(), *GetNameSafe(SourceEquipment), *Resolved.ToString());
					return Resolved;
				}
			}

			// Side-correct fallback: this equipment's own actor, never the other cannon's barrel,
			// never the shared weapon_r hand socket, never world origin.
			UE_LOG(LogAFLCombat, Verbose,
				TEXT("AFL_LASER/MUZZLE(side): no Muzzle socket on %s -> its actor origin"),
				*Spawned->GetName());
			return Spawned->GetActorLocation();
		}
	}

	// No equipment (activate-by-class harness, bot GameplayEvent fire, no current spec) -> the proven
	// pawn-scoped path below, byte-for-byte unchanged.
	return ResolveMuzzleLocation(AvatarPawn);
}

FVector UAFLAG_Laser_Beam::ResolveMuzzleLocation(APawn* AvatarPawn) const
{
	// Verbatim Pulse's proven UAFLAG_Laser_Pulse::ResolveMuzzleLocation. Fallback to the
	// weapon_r hand socket so the published muzzle is NEVER origin (un-armed, or a weapon
	// without the "Muzzle" socket convention -- e.g. the Beam mesh until a socket is authored).
	// That fallback is what makes wiring this safe with no regression: worst case the beam
	// emits from the hand, never vanishes / never shoots from world origin.
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
	for (AActor* Attached : AttachedActors)
	{
		TInlineComponentArray<UStaticMeshComponent*> SMCs;
		Attached->GetComponents<UStaticMeshComponent>(SMCs);
		for (UStaticMeshComponent* SMC : SMCs)
		{
			if (SMC && SMC->DoesSocketExist(FName("Muzzle")))
			{
				return SMC->GetSocketLocation(FName("Muzzle"));
			}
		}
	}

	// Path B: Lyra equipment attaches the weapon to Char->GetMesh(), not the pawn root, so
	// walk the character mesh's descendant components for the Muzzle-socketed SMC.
	if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
	{
		if (USkeletalMeshComponent* CharMesh = AvatarChar->GetMesh())
		{
			TArray<USceneComponent*> MeshChildren;
			CharMesh->GetChildrenComponents(/*bIncludeAllDescendants=*/true, MeshChildren);
			for (USceneComponent* Child : MeshChildren)
			{
				if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
				{
					if (SMC->DoesSocketExist(FName("Muzzle")))
					{
						return SMC->GetSocketLocation(FName("Muzzle"));
					}
				}
			}
		}
	}

	// No runtime-resolvable Muzzle socket -- the weapon_r hand fallback stands (never origin).
	// For the Beam this path is not taken: tripo_part_0's Muzzle socket resolves at runtime
	// (PIE-verified 12/12, AFL-0208), so the start is barrel-accurate.
	return MuzzleLocation;
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

	// PER-INSTANCE HEAT RAMP (Block 19) -- runs on BOTH client and server, off the same per-tick target
	// data, so the predicting client and the authoritative server reach the cap on the SAME tick without
	// replicating anything. This is the hitscan auto-fire model; it replaces applying GE_AFL_Heat_BeamTick
	// + GE_AFL_Heat_CoolingGate to the shared ASC Heat pool (see the header for why the pool couples the
	// two arm-cannons). Set BEFORE the dispatch below so ServerApplyTargetData sees bOverheated and ends
	// the channel WITHOUT applying this tick's damage -- preserving the boundary the old ASC-tag check
	// guarded ("no squeezing an extra damage tick out of the overheat boundary").
	//
	// The payload is still shipped/dispatched on the overheating tick: cutting it short here would leave
	// the server one tick behind the client forever, so the server's own ramp would never reach the cap
	// and the lockout would stop being server-authoritative.
	if (AdvanceHeat() >= 1.0f)
	{
		bOverheated = true;
	}

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

	// ...with ONE exception: overheat on a REMOTE client. ServerApplyTargetData force-ends the channel,
	// but that only runs on authority, so a remote client would keep channelling until the server's
	// EndAbility replicated back. Ending locally makes overheat feel instant and matches the prediction
	// the server is about to confirm. The listen-server host is bIsAuthority, already ended above.
	if (bOverheated && !bIsAuthority && bIsLocallyControlled)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BEAM: overheat (client predict) — ending channel"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			/*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
	}
}

#if WITH_SERVER_CODE
void UAFLAG_Laser_Beam::ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data)
{
	UAbilitySystemComponent* SourceASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC || !DamageEffectClass)
	{
		return;
	}

	// BLOCK 19: the per-tick heat GEs that used to run here are GONE from this ability's hot path.
	// GE_AFL_Heat_BeamTick (+4) and GE_AFL_Heat_CoolingGate (0.5s decay suppressor) both wrote the
	// SHARED per-ASC Heat pool, so two arm-cannons filled ONE gauge at double rate and tripped the
	// pawn-wide State.Overheated together. OnTargetDataReadyCallback now ramps this ability instance's
	// own HeatNorm instead (same 0.04/tick, same 0.5s grace, same 25-tick/2.5s cap), so each cannon
	// overheats and vents on its own clock.
	//
	// Both GE classes, the Heat attributes and the afl.Combat.* cheats remain in place and functional --
	// they are simply no longer driven by beam fire. HeatDecayEffectClass is STILL ensured on the ASC in
	// ActivateAbility, so cheat-set Heat continues to decay and clear State.Overheated as before.

	// Mid-channel overheat check, now on this instance's own flag rather than the pawn-wide
	// State.Overheated tag (which would have force-ended BOTH cannons when either one capped). Runs
	// before the damage GEs so the player still can't squeeze an extra damage tick out of the overheat
	// boundary. ActivationBlockedTags only gates re-entry — once an ability is already active the engine
	// doesn't re-check those tags, which is why this explicit check has to exist at all.
	if (bOverheated)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BEAM: overheat — ending channel"));
		// EndAbility now applies the release cooldown for EVERY channel-end (single apply point),
		// so no explicit ApplyReleaseCooldown() here -- it would double-apply. The player still
		// can't immediately re-channel once venting clears (the cooldown lands in EndAbility below).
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

		// AFL-0208 (published-value): on the AUTHORITY, publish the endpoint from the
		// replicated payload into the beam-channel bridge so simulated proxies (other
		// clients) replicate it and their cue beams track. We publish the ImpactPoint even
		// on a whiff (HitResult carries the trace end when nothing was hit), so the beam
		// renders to the far point rather than freezing. The locally-controlled side already
		// published immediately in TickChannel; this is the remote-client path. Cosmetic
		// only -- it never gates the damage application below.
		if (RawData->GetScriptStruct() == FAFLAbilityTargetData_Hitscan::StaticStruct())
		{
			if (UAFLBeamChannelComponent* Channel = ResolveBeamChannel())
			{
				Channel->PublishImpact(static_cast<const FAFLAbilityTargetData_Hitscan*>(RawData)->HitResult.ImpactPoint);
				// Publish the muzzle on the authority too (the visible START), so simulated
				// proxies emit the beam from the barrel. Resolved from the avatar pawn.
				Channel->PublishMuzzle(ResolveMuzzleLocation(GetAFLLaserWeaponInstance(), Cast<APawn>(CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr)));
			}
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

		// Per-tick base damage, sourced from the GE's first Damage modifier so BP children that
		// tune it stay in sync (falls back to 1.2 if the GE has a non-standard shape). Computed
		// HERE -- before MakeOutgoingSpec -- because it must seed Source.Damage (below).
		float DamageMagnitude = 1.2f;
		if (const UGameplayEffect* GECdo = DamageEffectClass.GetDefaultObject())
		{
			if (GECdo->Modifiers.Num() > 0)
			{
				GECdo->Modifiers[0].ModifierMagnitude.GetStaticMagnitudeIfPossible(
					GetAbilityLevel(), DamageMagnitude);
			}
		}

		// Seed Source.Damage on the firing ASC BEFORE MakeOutgoingSpec -- the SAME requirement
		// the Pulse path has (BM-0102). UAFLDamageExecCalc captures Source.Damage with
		// bSnapshot=true at spec creation; without this seed it captures 0 -> EffectiveDamage<=0
		// -> the ExecCalc's mitigated-reject early-return -> NO Health output modifier -> the
		// dummy's Health never drops (the BM-0103 "beam logs 1.2 damage but never kills" bug).
		// Override semantics fully own the value per-tick. Mirrors AFLAG_Laser_Pulse:689-697.
		SourceASC->ApplyModToAttribute(
			UAFLAttributeSet_Combat::GetDamageAttribute(),
			EGameplayModOp::Override,
			DamageMagnitude);

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

		// SetByCaller multipliers the ExecCalc reads (default 1.0f when absent) -- set explicitly
		// for predictable runs + the Headshot/Weakpoint/Distance tuning hooks. Mirrors Pulse.
		FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Headshot_Beam,  false), 1.0f);
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Weakpoint_Beam, false), 1.0f);
		Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Distance_Beam,  false), 1.0f);

		SourceASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC);

		// Successful-tick telemetry. NOTE: this logs the seeded base magnitude (intent); the
		// ACTUAL health delta is the ExecCalc's output (LogGameplayEffects Health line) -- the
		// two diverged in the BM-0103 bug (this logged 1.2 while Health never moved). Now that
		// Source.Damage is seeded they agree.
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

// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_BeamChannel_v2.h"

#include "AFLCombat.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Beam/AFLBeamChannelComponent.h"
#include "Beam/AFLBeamVisualComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Effects/GE_AFL_Cooldown_Beam.h"
#include "Effects/GE_AFL_Damage_BeamTick.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "Targeting/AFLAbilityTargetData_Hitscan.h"
#include "TimerManager.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_BeamChannel_v2)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_BeamV2, "State.Carrying");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_ThrowRecovery_BeamV2, "State.Weapon.ThrowRecovery");

// Gated-heat force-end tags (Beam Cutter, weapon #8). Overheated: grant/clear is owned by
// UAFLAttributeSet_Combat (replicated loose tag at Heat==MaxHeat, cleared below MaxHeat*0.3 by the
// decay path). Weapon.Disabled: granted by the EMP's GE_AFL_EMP_Disable -- checking it mid-channel
// makes weapon #7 cut weapon #8's ACTIVE beam (deliberate cross-weapon counterplay; the inherited
// ActivationBlockedTags entry from Laser_Base only gates re-entry, never a running channel).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Overheated_BeamV2, "State.Overheated");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Weapon_Disabled_BeamV2, "State.Weapon.Disabled");

// SetByCaller magnitude tags consumed by UAFLDamageExecCalc (default 1.0f when absent). v2 seeds
// these alongside Source.Damage so the ExecCalc has a non-zero base to compute the health delta --
// WITHOUT the Source.Damage seed below, the ExecCalc captures Source.Damage=0 and the tick is fully
// mitigated (no Health change). That is the "beam shows but raw=0.0 / dummy never dies" bug (BM-0103),
// which v2 reproduced until this seed was added. EXACT mirror of AFLAG_Laser_Beam:57-59 (proven path).
// Per-file symbol suffix (_BeamV2) is Unity-build safe; the FName *value* stays canonical "Data.Damage.*".
namespace
{
	const FName NAME_Data_Damage_Headshot_BeamV2  = TEXT("Data.Damage.Headshot");
	const FName NAME_Data_Damage_Weakpoint_BeamV2 = TEXT("Data.Damage.Weakpoint");
	const FName NAME_Data_Damage_Distance_BeamV2  = TEXT("Data.Damage.Distance");
}

UAFLAG_BeamChannel_v2::UAFLAG_BeamChannel_v2()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// WhileInputActive ALONE owns the held lifecycle (the thrash lesson): Lyra's ProcessAbilityInput
	// activates once while held; CancelInputActivatedAbilities ends it on release. NO explicit
	// UAbilityTask_WaitInputRelease (that re-cycled ~6x/sec). Release-cooldown applies in EndAbility.
	ActivationPolicy = ELyraAbilityActivationPolicy::WhileInputActive;

	// Throw cycle: no firing while carrying (the holstered rifle is hidden, not unequipped; LMB belongs
	// to the throw ability under State.Carrying -- same arbitration as Pulse/Beam).
	ActivationBlockedTags.AddTag(TAG_State_Carrying_BeamV2);

	// ...and no channeling from the press/hold that THREW: GE_AFL_ThrowRecovery's 0.4s tag window. The
	// WhileInputActive policy re-tries every held frame -- this was the PIE-caught beam-after-throw
	// ([N] throw -> [N+2] beam, deterministic): IA_Weapon_Fire's trigger reports release one frame after
	// the press even while held, so only a time-based gate holds.
	ActivationBlockedTags.AddTag(TAG_State_ThrowRecovery_BeamV2);

	// Overheat lockout (Beam Cutter): re-entry stays blocked while venting, until the drain drops
	// Heat below MaxHeat*0.3 and the AttributeSet clears the replicated tag. INERT for the live
	// ShotgunBeam -- its heat knobs are null, so the tag never exists on its wielder. (The EMP's
	// State.Weapon.Disabled re-entry block is already inherited from UAFLAG_Laser_Base.)
	ActivationBlockedTags.AddTag(TAG_State_Overheated_BeamV2);

	DamageEffectClass          = UGE_AFL_Damage_BeamTick::StaticClass();
	ReleaseCooldownEffectClass = UGE_AFL_Cooldown_Beam::StaticClass();

	// CharacterFireMontage default. NOT picked blindly: the canonical / most-common beam weapon (the
	// live Beam_v2) is a B_WeaponInstance_Rifle child -- 2H rifle-class -- so the rifle 2H fire brace is
	// the class-correct SHARED default that every beam weapon inherits. AM_MM_Rifle_Fire is an
	// AAT_ROTATION_OFFSET_MESH_SPACE ADDITIVE that LAYERS on the held aim pose (proven on the Carbine,
	// 66ed384d). It plays once at beam-start via the GAS-replicated PlayMontage already wired into
	// ActivateAbility this turn -> remote clients see the brace by construction, no new plumbing.
	// DOCUMENTED OVERRIDE POINT: a non-rifle beam chassis sets its own montage on its BP child -- the
	// Shotgun-Beam child overrides this to AM_MM_Shotgun_Fire (now a trap-free modify-existing, because
	// this base default is non-null).
	static ConstructorHelpers::FObjectFinder<UAnimMontage> BeamFireMontageFinder(
		TEXT("/Game/Weapons/Rifle/Animations/AM_MM_Rifle_Fire.AM_MM_Rifle_Fire"));
	if (BeamFireMontageFinder.Succeeded())
	{
		CharacterFireMontage = BeamFireMontageFinder.Object;
	}
}

void UAFLAG_BeamChannel_v2::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BEAMV2: Activate"));

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// GATED HEAT drain-ensure (Beam Cutter): authority idempotently ensures the Infinite
	// Heat_Decay GE is resident on the source ASC -- skip if one is already active. Verbatim
	// transplant of the retired beam's proven ensure (AFLAG_Laser_Beam::ActivateAbility, AFL-0207).
	// Null knob (ShotgunBeam) = this whole block is skipped.
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

	// Character fire montage (2H brace / trigger-pull), played ONCE at beam-start via the GAS-
	// replicated PlayMontage path. ActivateAbility runs on the autonomous proxy (predict) + the
	// authority for this LocalPredicted ability -- the authority's RepAnimMontageInfo replicates the
	// montage to sim proxies, so REMOTE clients see the brace, not just the firing client. This rides
	// the SAME already-replicated delivery the beam visual uses (replicated UAFLBeamVisualComponent +
	// server-auth endpoint) -- no new plumbing. Role-agnostic in the activation prediction window
	// (Pulse's proven montage shape, 66ed384d). The base ctor defaults this non-null (Option A: the rifle
	// 2H brace), so the live Beam_v2 inherits a brace too -- accepted, it is the discard candidate.
	if (CharacterFireMontage)
	{
		ASC->PlayMontage(this, CurrentActivationInfo, CharacterFireMontage, 1.0f);
	}

	// Bind the target-data delegate on BOTH sides (same pattern as the proven beam).
	OnTargetDataReadyCallbackDelegateHandle =
		ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
		   .AddUObject(this, &ThisClass::OnTargetDataReadyCallback);

	// Listen for input release -> end the channel. RESTORED from the control beam's pre-3bf573b3
	// wiring (verbatim template). Lyra source ground-truth: ProcessAbilityInput's release path only
	// fires the InputReleased EVENT; NOTHING in the input pipeline calls CancelInputActivatedAbilities
	// on release (its sole caller is the CheatManager). So WhileInputActive owns activate+sustain but
	// NOT end-on-release -- the ability MUST listen for InputReleased to end itself. Without this the
	// channel runs forever (only EndAbility-on-teardown), which is exactly the v2 stuck-on bug.
	// NOT a thrash risk: IA_Weapon_Fire_Auto is empty-trigger/held, so InputReleased fires ONCE on
	// real button-up (not mid-hold) -> one clean Activate->Release. (The original thrash was the
	// Pressed-trigger instant-loop, NOT this task -- see git 3bf573b3 reconciliation.)
	if (UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(
			this, /*bTestAlreadyReleased=*/false))
	{
		ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
		ReleaseTask->ReadyForActivation();
	}

	// Q2=(a) DELIVERY: open the beam on the weapon-actor's visual component. AUTHORITY-only --
	// SetBeamActive sets the replicated bBeamActive AND applies the visual locally on the server
	// (the listen-host-as-player toggles; OnRep won't fire on the server), then OnRep toggles every
	// remote proxy via the SAME shared ApplyBeamActiveState (Edge 1). NO GameplayCue spawn here.
	if (ActorInfo->IsNetAuthority())
	{
		if (UAFLBeamVisualComponent* Visual = ResolveWeaponVisual())
		{
			Visual->SetBeamActive(true);
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_BEAMV2: no UAFLBeamVisualComponent on the equipped weapon actor -- beam will not show."));
		}
	}

	if (ActorInfo->IsLocallyControlled())
	{
		// Locally-controlled (or listen-host): start the per-tick trace timer. Fire one immediately
		// so the channel produces an endpoint on frame 0, then settle into the interval.
		if (UWorld* World = ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld() : nullptr)
		{
			TickChannel();
			World->GetTimerManager().SetTimer(
				TickTimerHandle,
				FTimerDelegate::CreateUObject(this, &ThisClass::TickChannel),
				TickInterval,
				/*InbLoop=*/true);
		}
	}
}

void UAFLAG_BeamChannel_v2::EndAbility(
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

		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BEAMV2: Release"));

		// Release-cooldown — single channel-end point (fires whether Lyra cancelled the held channel
		// on input-release, or it self-cancelled). Authority-gated inside.
		ApplyReleaseCooldown();

		// Stop the tick timer.
		if (TickTimerHandle.IsValid() && ActorInfo && ActorInfo->AvatarActor.IsValid())
		{
			if (UWorld* World = ActorInfo->AvatarActor->GetWorld())
			{
				World->GetTimerManager().ClearTimer(TickTimerHandle);
			}
		}

		// Q2=(a): close the beam on the weapon-actor visual (authority -> replicated bBeamActive=false
		// -> OnRep Deactivates on every proxy; server applies locally). Mirrors the open in Activate.
		if (ActorInfo && ActorInfo->IsNetAuthority())
		{
			if (UAFLBeamVisualComponent* Visual = WeaponVisual.IsValid() ? WeaponVisual.Get() : ResolveWeaponVisual())
			{
				Visual->SetBeamActive(false);
			}
		}

		// Unbind the target-data delegate + clear the per-key cache.
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
			ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
			   .Remove(OnTargetDataReadyCallbackDelegateHandle);
			ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
		}

		// Close the published-value bridge (the endpoint side). The visual component stops reading
		// once Deactivated; clearing bBeamActive on the bridge is the authoritative "off" for any
		// other consumer. (The bridge stays on the pawn; only its active flag flips.)
		if (UAFLBeamChannelComponent* Channel = BeamChannel.Get())
		{
			Channel->SetBeamActive(false);
		}
		BeamChannel.Reset();
		WeaponVisual.Reset();

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

void UAFLAG_BeamChannel_v2::TickChannel()
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

	// Local-side force-end mirror (the AUTHORITATIVE check + the heat build live in
	// ServerApplyTargetData): if the replicated State.Overheated / State.Weapon.Disabled tag has
	// landed on this ASC, drop the channel NOW rather than one server round-trip later -- the
	// EMP'd player's beam dies the frame the disable arrives on their machine. Inert while both
	// tags are absent (ShotgunBeam with null heat knobs + no EMP hit = two failed lookups).
	if (ASC->HasMatchingGameplayTag(TAG_State_Overheated_BeamV2)
		|| ASC->HasMatchingGameplayTag(TAG_State_Weapon_Disabled_BeamV2))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			/*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// Camera-aligned origin/direction (AFL-0215: read the camera manager surface, not the
	// controller view-helper). Same as the proven beam.
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

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLBeamV2Hitscan), /*bTraceComplex=*/true);
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

	// Q1=(b): publish the authoritative endpoint + muzzle into the pawn's bridge (locally immediate;
	// authority publishes the same from ServerApplyTargetData for proxies). The weapon-actor visual
	// component READS these on its own tick (Edge 2: replicated cadence, not per-frame).
	if (UAFLBeamChannelComponent* Channel = ResolveBeamChannel())
	{
		Channel->PublishImpact(Hit.ImpactPoint);
		Channel->PublishMuzzle(ResolveMuzzleLocation(AvatarPawn));
	}

	// Reuse the proven hitscan payload (AFLNetTypes) — no per-weapon fork.
	FAFLAbilityTargetData_Hitscan* NewTargetData = new FAFLAbilityTargetData_Hitscan();
	NewTargetData->HitResult                   = Hit;
	NewTargetData->ClaimedViewOrigin           = ViewLocation;
	NewTargetData->ClaimedAimDirection         = AimDirection;
	NewTargetData->AimAngularVelocityDegPerSec = 0.0f;

	FGameplayAbilityTargetDataHandle TargetDataHandle;
	TargetDataHandle.Add(NewTargetData);

	{
		FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());
		OnTargetDataReadyCallback(TargetDataHandle, FGameplayTag());
	}
}

void UAFLAG_BeamChannel_v2::OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag)
{
	check(CurrentActorInfo);
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	check(ASC);

	if (!ASC->FindAbilitySpecFromHandle(CurrentSpecHandle))
	{
		return;
	}

	FGameplayAbilityTargetDataHandle LocalTargetDataHandle(
		MoveTemp(const_cast<FGameplayAbilityTargetDataHandle&>(InData)));

	const bool bIsAuthority         = CurrentActorInfo->IsNetAuthority();
	const bool bIsLocallyControlled = CurrentActorInfo->IsLocallyControlled();

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

	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
}

#if WITH_SERVER_CODE
void UAFLAG_BeamChannel_v2::ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data)
{
	UAbilitySystemComponent* SourceASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC || !DamageEffectClass)
	{
		return;
	}

	// Authority publishes the endpoint too, so simulated proxies get it (the weapon-actor visual
	// on the proxy reads the bridge). Mirrors the local publish in TickChannel.
	if (UAFLBeamChannelComponent* Channel = ResolveBeamChannel())
	{
		for (int32 Index = 0; Index < Data.Num(); ++Index)
		{
			if (const FGameplayAbilityTargetData* RawData = Data.Get(Index))
			{
				if (RawData->GetScriptStruct() == FAFLAbilityTargetData_Hitscan::StaticStruct())
				{
					const FAFLAbilityTargetData_Hitscan* Hitscan = static_cast<const FAFLAbilityTargetData_Hitscan*>(RawData);
					Channel->PublishImpact(Hitscan->HitResult.ImpactPoint);
					Channel->PublishMuzzle(ResolveMuzzleLocation(Cast<APawn>(CurrentActorInfo->AvatarActor.Get())));
					break;
				}
			}
		}
	}

	// GATED HEAT (Beam Cutter, weapon #8) -- the retired beam's PROVEN server block
	// (AFLAG_Laser_Beam::ServerApplyTargetData, AFL-0207) transplanted behind default-null knobs:
	// gate + build run on every authoritative tick REGARDLESS of hit (placed BEFORE the whiff
	// early-return below -- heat is a hold-time budget; off-target sweeping can't dodge the
	// limiter), and BEFORE the damage apply so no extra damage tick squeezes past the overheat
	// boundary. All three blocks compile to skipped branches for the live ShotgunBeam (null knobs).
	//
	// CoolingGate first: removed-then-reapplied so every tick produces a FRESH 0.5s suppression
	// window over Heat_Decay -- DurationPolicy alone doesn't refresh existing actives.
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

	if (HeatPerTickEffectClass)
	{
		FGameplayEffectContextHandle HeatContext = SourceASC->MakeEffectContext();
		HeatContext.AddInstigator(CurrentActorInfo->OwnerActor.Get(), CurrentActorInfo->AvatarActor.Get());
		FGameplayEffectSpecHandle HeatSpec =
			SourceASC->MakeOutgoingSpec(HeatPerTickEffectClass, GetAbilityLevel(), HeatContext);
		if (HeatSpec.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToSelf(*HeatSpec.Data.Get());
		}
	}

	// Mid-channel force-end (the retired beam's proven line). ActivationBlockedTags only gate
	// RE-entry -- the engine never re-checks them on a running ability -- so an active channel must
	// self-end when (a) the heat tick above just drove Heat to MaxHeat (State.Overheated), or
	// (b) the EMP disabled this player mid-cut (State.Weapon.Disabled -- weapon #7 cuts weapon #8's
	// active channel, both ways deliberate). EndAbility owns the release cooldown (single apply
	// point) and replicates the end down to the owning client.
	if (SourceASC->HasMatchingGameplayTag(TAG_State_Overheated_BeamV2)
		|| SourceASC->HasMatchingGameplayTag(TAG_State_Weapon_Disabled_BeamV2))
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BEAMV2: force-end (overheat/disabled) -- ending channel"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			/*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// Per-tick damage through the proven ExecCalc pipeline. EXACT mirror of the proven per-tick beam
	// AFLAG_Laser_Beam::ServerApplyTargetData:640-685 (which itself mirrors Pulse). The earlier v2 code
	// here called MakeOutgoingSpec WITHOUT seeding Source.Damage first -> ExecCalc captured 0 -> every
	// tick logged raw=0.0 reason=mitigated -> beam showed but dealt no damage (BM-0103). Do NOT invent a
	// damage path; the foundation was kept so the new firing layer plugs in the SAME proven way.
	const FAFLAbilityTargetData_Hitscan* HitscanData = nullptr;
	for (int32 Index = 0; Index < Data.Num(); ++Index)
	{
		if (const FGameplayAbilityTargetData* RawData = Data.Get(Index))
		{
			if (RawData->GetScriptStruct() == FAFLAbilityTargetData_Hitscan::StaticStruct())
			{
				HitscanData = static_cast<const FAFLAbilityTargetData_Hitscan*>(RawData);
				break;
			}
		}
	}
	if (!HitscanData || !HitscanData->HitResult.bBlockingHit)
	{
		return;
	}

	AActor* HitActor = HitscanData->HitResult.GetActor();
	UAbilitySystemComponent* TargetASC = HitActor
		? UAbilitySystemGlobals::Get().GetAbilitySystemComponentFromActor(HitActor)
		: nullptr;
	if (!TargetASC)
	{
		return;
	}

	// Per-tick base damage, sourced from DamageEffectClass's first Damage modifier so BP children that
	// tune it stay in sync (fallback 1.2 if the GE has a non-standard shape). Computed HERE -- before
	// MakeOutgoingSpec -- because it must seed Source.Damage (below). Mirrors AFLAG_Laser_Beam:640-651.
	float DamageMagnitude = 1.2f;
	if (const UGameplayEffect* GECdo = DamageEffectClass.GetDefaultObject())
	{
		if (GECdo->Modifiers.Num() > 0)
		{
			GECdo->Modifiers[0].ModifierMagnitude.GetStaticMagnitudeIfPossible(GetAbilityLevel(), DamageMagnitude);
		}
	}

	// Seed Source.Damage on the firing ASC BEFORE MakeOutgoingSpec -- UAFLDamageExecCalc captures
	// Source.Damage with bSnapshot=true at spec creation; without this seed it captures 0 -> the
	// ExecCalc's mitigated-reject early-return -> no Health output -> raw=0.0 in telemetry. Override
	// semantics fully own the value per-tick. Mirrors AFLAG_Laser_Beam:659-662 / Pulse:705-708.
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
			TEXT("AFL_BEAMV2: MakeOutgoingSpec(%s) returned invalid handle"),
			*GetNameSafe(DamageEffectClass));
		return;
	}

	// SetByCaller multipliers the ExecCalc reads (default 1.0f when absent) -- set explicitly for
	// predictable runs + the Headshot/Weakpoint/Distance tuning hooks. Mirrors AFLAG_Laser_Beam:680-683.
	FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Headshot_BeamV2,  false), 1.0f);
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Weakpoint_BeamV2, false), 1.0f);
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Distance_BeamV2,  false), 1.0f);

	SourceASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC);
}
#endif

void UAFLAG_BeamChannel_v2::OnInputReleased(float /*TimeHeld*/)
{
	// Bound to UAbilityTask_WaitInputRelease::OnRelease (fires on BOTH sides -- the engine replicates
	// the input-release event up; the server runs its own task copy in parallel). End the channel here.
	// NOTE: unlike the control beam's pre-3bf573b3 OnInputReleased, this does NOT call
	// ApplyReleaseCooldown() -- v2's EndAbility already applies it as the single channel-end point
	// (mirrors the post-3bf573b3 control). Calling it here too would DOUBLE-APPLY the cooldown.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		/*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

// ResolveMuzzleLocation moved to UAFLAG_Laser_Base -- the ONE shared resolver Pulse and v2 both
// inherit. This copy was UStaticMeshComponent-only; the harvest-clone shotgun chassis is SKELETAL
// (SKM_Shotgun), so it read no muzzle here and fell back to weapon_r -- the barrel-up/beam-down V.
// The base resolver (UMeshComponent + {"Muzzle","Barrel","Slide"}) resolves SKM_Shotgun's real
// "Muzzle" socket, and the result still flows out through the existing replicated PublishMuzzle path
// (TickChannel + ServerApplyTargetData -> UAFLBeamChannelComponent), so sim proxies get the barrel-
// accurate start too. This is the fold that makes the no-twin guarantee real for the LIVE beam.

void UAFLAG_BeamChannel_v2::ApplyReleaseCooldown()
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
	// Authority-only — cooldowns are server-authoritative and replicate down via the GE's tags.
	if (!CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(CurrentActorInfo->OwnerActor.Get(), CurrentActorInfo->AvatarActor.Get());

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ReleaseCooldownEffectClass, GetAbilityLevel(), Context);
	if (SpecHandle.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

UAFLBeamChannelComponent* UAFLAG_BeamChannel_v2::ResolveBeamChannel()
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
		// Authority-only create (replicated, stable name) — propagates to all clients incl. proxies.
		Channel = NewObject<UAFLBeamChannelComponent>(Avatar, TEXT("AFLBeamChannel"));
		Channel->RegisterComponent();
	}
	if (Channel)
	{
		BeamChannel = Channel;
	}
	return Channel;
}

UAFLBeamVisualComponent* UAFLAG_BeamChannel_v2::ResolveWeaponVisual() const
{
	if (UAFLBeamVisualComponent* Cached = WeaponVisual.Get())
	{
		return Cached;
	}
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn)
	{
		return nullptr;
	}

	// The cross-actor reach (pawn-ability -> weapon display actor -> visual component). Walk the
	// pawn's attached actors for the one carrying the visual component. This is the bridge the
	// shipping system needs; proving it works is the canary's point.
	TArray<AActor*> AttachedActors;
	AvatarPawn->GetAttachedActors(AttachedActors);
	for (AActor* Attached : AttachedActors)
	{
		if (UAFLBeamVisualComponent* Visual = Attached->FindComponentByClass<UAFLBeamVisualComponent>())
		{
			const_cast<UAFLAG_BeamChannel_v2*>(this)->WeaponVisual = Visual;
			return Visual;
		}
	}
	return nullptr;
}

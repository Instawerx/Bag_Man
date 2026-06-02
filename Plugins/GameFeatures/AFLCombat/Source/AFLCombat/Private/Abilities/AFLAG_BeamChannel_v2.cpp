// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_BeamChannel_v2.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
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
#include "Targeting/AFLAbilityTargetData_Hitscan.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_BeamChannel_v2)

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

	DamageEffectClass          = UGE_AFL_Damage_BeamTick::StaticClass();
	ReleaseCooldownEffectClass = UGE_AFL_Cooldown_Beam::StaticClass();
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

	// Bind the target-data delegate on BOTH sides (same pattern as the proven beam).
	OnTargetDataReadyCallbackDelegateHandle =
		ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey())
		   .AddUObject(this, &ThisClass::OnTargetDataReadyCallback);

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

FVector UAFLAG_BeamChannel_v2::ResolveMuzzleLocation(APawn* AvatarPawn) const
{
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

	// Path A: pawn's attached actors (the weapon display actor) — find the "Muzzle"-socketed SMC.
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

	// Path B: walk the character mesh's descendants (Lyra attaches to Char->GetMesh()).
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
	return MuzzleLocation;
}

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

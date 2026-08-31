// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHubNetProfileComponent.h"

#include "TimerManager.h" // deferred pawn frequency decision

#include "AFLHub.h"
#include "AFLHubZoneProfiles.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h" // NO-KILL law: dynamic-tag GE (the God-cheat mechanism)
#include "System/LyraAssetManager.h"                   // NO-KILL: resolve the dynamic-tag GE class
#include "System/LyraGameData.h"                       // NO-KILL: DynamicTagGameplayEffect soft ref
#include "GameplayEffect.h"                            // NO-KILL: spec build on the raw ASC
#include "Character/LyraHealthComponent.h"            // respawn safety net: death watch
#include "GameModes/LyraGameMode.h"                    // respawn safety net: RequestPlayerRestartNextFrame
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Engine/ReplicatedState.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHubNetProfileComponent)

UAFLHubNetProfileComponent::UAFLHubNetProfileComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // event-driven (tag listener), not tick-driven.
}

void UAFLHubNetProfileComponent::BeginPlay()
{
	Super::BeginPlay();

	// Frequency + quantisation land unconditionally (server writes the wire format; the owning
	// client's copy keeps CDO expectations coherent -- the AC's "authority + owning client").
	ApplyNetPosture();

	// HUB RESPAWN SAFETY NET: DamageImmunity (below) stops weapon damage, but Lyra's SelfDestruct
	// path (KillZ / fell-out-of-world) bypasses immunity BY DESIGN -- and the hub experience runs no
	// shooter respawn rule, so a dead player sat dead forever (lap-3). Authority watches the pawn's
	// death and requests the stock Lyra restart.
	if (AActor* NetOwner = GetOwner(); NetOwner && NetOwner->HasAuthority())
	{
		if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(NetOwner))
		{
			Health->OnDeathFinished.AddDynamic(this, &UAFLHubNetProfileComponent::HandleDeathFinished);
		}
	}

	// ASC resolve: DIRECT first, PawnExtension hook as the fallback for the possessed PLAYER whose
	// PlayerState ASC lands after pawn BeginPlay. The Sprint/Dash/Death proven bind, verbatim.
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
		else if (ULyraPawnExtensionComponent* PawnExt = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Owner))
		{
			PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
				FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemReady));
		}
	}
}

void UAFLHubNetProfileComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromAbilitySystem();
	RestoreNetPosture();
	Super::EndPlay(EndPlayReason);
}

void UAFLHubNetProfileComponent::HandleDeathFinished(AActor* /*OwningActor*/)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	UWorld* World = GetWorld();
	ALyraGameMode* GameMode = World ? World->GetAuthGameMode<ALyraGameMode>() : nullptr;
	if (Controller && GameMode)
	{
		UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBNET: death on the lobby (immunity-bypassing source) -> respawn requested for %s."),
			*GetNameSafe(Controller));
		GameMode->RequestPlayerRestartNextFrame(Controller, /*bForceReset*/ true);
	}
	else
	{
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_HUBNET: death seen but no controller/GameMode to respawn (%s / %s)."),
			*GetNameSafe(Controller), *GetNameSafe(GameMode));
	}
}

void UAFLHubNetProfileComponent::OnAbilitySystemReady()
{
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
	}
}

void UAFLHubNetProfileComponent::BindToAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC || (CachedASC.Get() == InASC && ZoneTagHandles.Num() > 0))
	{
		return; // idempotent
	}
	if (CachedASC.IsValid() && CachedASC.Get() != InASC)
	{
		UnbindFromAbilitySystem(); // controller swap -> fresh PlayerState ASC
	}

	// NO-KILL LAW (operator-ruled 2026-08-31, distributed retail S2): "test-fire yes but no players
	// can die on our Lobby map." Every hub player gets Gameplay.DamageImmunity -- ULyraHealthSet
	// zeroes ALL incoming damage on a target holding it (the AFL exec calc emits health damage
	// through that same meta attribute), while firing stays fully live. Applied as the SAME
	// dynamic-tag GE the God cheat uses -- but spec-built directly on the RAW ASC: lap-3 died
	// because the pawn's ASC is not guaranteed to be ULyraAbilitySystemComponent (self-ASC'd pawn
	// law) and the old Cast<> guard SKIPPED SILENTLY. Every refusal branch now logs; sits ABOVE the
	// ZoneProfiles gate so the law holds even on a pawn with no zone DA. Idempotent via the tag check.
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		const FGameplayTag Immunity = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.DamageImmunity"), false);
		if (!Immunity.IsValid())
		{
			UE_LOG(LogAFLHub, Error, TEXT("AFL_HUBNET: NO-KILL FAILED -- Gameplay.DamageImmunity tag not registered."));
		}
		else if (InASC->HasMatchingGameplayTag(Immunity))
		{
			UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBNET: NO-KILL already held by %s."), *GetNameSafe(OwnerActor));
		}
		else if (const TSubclassOf<UGameplayEffect> DynamicTagGE =
			ULyraAssetManager::GetSubclass(ULyraGameData::Get().DynamicTagGameplayEffect))
		{
			const FGameplayEffectSpecHandle SpecHandle = InASC->MakeOutgoingSpec(DynamicTagGE, 1.0f, InASC->MakeEffectContext());
			if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
			{
				Spec->DynamicGrantedTags.AddTag(Immunity);
				// Lobby decorum rider: the AFL exec calc guards only LYRA health -- zone drains could
				// still SEVER an immune player's limb mid-test-fire. The calc honors this tag; grant
				// it in the same effect so no-kill also means no-dismember on the lobby.
				const FGameplayTag NoDismember = FGameplayTag::RequestGameplayTag(TEXT("State.Mode.NoDismember"), false);
				if (NoDismember.IsValid())
				{
					Spec->DynamicGrantedTags.AddTag(NoDismember);
				}
				InASC->ApplyGameplayEffectSpecToSelf(*Spec);
				UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBNET: NO-KILL law applied to %s (DamageImmunity now %s)."),
					*GetNameSafe(OwnerActor),
					InASC->HasMatchingGameplayTag(Immunity) ? TEXT("HELD") : TEXT("MISSING -- INVESTIGATE"));
			}
			else
			{
				UE_LOG(LogAFLHub, Error, TEXT("AFL_HUBNET: NO-KILL FAILED -- could not build the dynamic-tag spec."));
			}
		}
		else
		{
			UE_LOG(LogAFLHub, Error, TEXT("AFL_HUBNET: NO-KILL FAILED -- DynamicTagGameplayEffect missing from LyraGameData."));
		}
	}

	if (!ZoneProfiles)
	{
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_HUBNET: %s has no ZoneProfiles DA -- frequency/quantisation applied, zone culling inert."),
			*GetNameSafe(GetOwner()));
		return;
	}

	CachedASC = InASC;
	for (const FAFLHubZoneProfile& Row : ZoneProfiles->Zones)
	{
		if (!Row.ZoneTag.IsValid())
		{
			continue;
		}
		FDelegateHandle Handle = InASC->RegisterGameplayTagEvent(Row.ZoneTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UAFLHubNetProfileComponent::HandleZoneTagChanged);
		ZoneTagHandles.Emplace(Row.ZoneTag, Handle);
	}
	UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBNET: %s bound %d zone-tag listeners (ASC %s)."),
		*GetNameSafe(GetOwner()), ZoneTagHandles.Num(), *GetNameSafe(InASC));

	// The pawn may already stand in a zone (spawn inside Hub.Zone.Main; the volume's BeginPlay seed
	// can beat this bind) -- sync to present tag state instead of waiting for the next transition.
	RecomputeCullDistance();
}

void UAFLHubNetProfileComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : ZoneTagHandles)
		{
			if (Pair.Value.IsValid())
			{
				ASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
			}
		}
	}
	ZoneTagHandles.Empty();
	CachedASC.Reset();
}

void UAFLHubNetProfileComponent::HandleZoneTagChanged(const FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
	RecomputeCullDistance();
}

void UAFLHubNetProfileComponent::RecomputeCullDistance()
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!Owner || !ASC || !ZoneProfiles || !Owner->HasAuthority())
	{
		return; // relevancy is a server decision; clients keep their applied posture untouched.
	}

	// Recomputed from LIVE tag state each change: idempotent across overlap seams, double-fires and
	// respawn-in-zone. Smallest active distance wins (the denser zone's perf posture is the safe one).
	float Distance = ZoneProfiles->DefaultNetCullDistance;
	FGameplayTag WinningTag;
	for (const FAFLHubZoneProfile& Row : ZoneProfiles->Zones)
	{
		if (Row.ZoneTag.IsValid() && ASC->GetTagCount(Row.ZoneTag) > 0
			&& (!WinningTag.IsValid() || Row.NetCullDistance < Distance))
		{
			Distance = Row.NetCullDistance;
			WinningTag = Row.ZoneTag;
		}
	}

	Owner->SetNetCullDistanceSquared(Distance * Distance);
	UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBNET: %s cull -> %.0fcm (%s)."),
		*GetNameSafe(Owner), Distance, WinningTag.IsValid() ? *WinningTag.ToString() : TEXT("no zone / default"));
}

void UAFLHubNetProfileComponent::ApplyNetPosture()
{
	AActor* Owner = GetOwner();
	if (!Owner || bPostureApplied)
	{
		return;
	}

	CachedNetUpdateFrequency = Owner->GetNetUpdateFrequency();
	CachedMinNetUpdateFrequency = Owner->GetMinNetUpdateFrequency();
	CachedNetCullDistanceSquared = Owner->GetNetCullDistanceSquared();

	// AFL-3012 DEVIATION (operator ruling 2026-08-30, "full fledge top tier movements"): the
	// helper-doc 15/5 Hz target was written for CROWD bandwidth, but applied to a PLAYER-
	// CONTROLLED pawn it throttles the owner's own correction loop -- measured in the hub walk
	// as "pulls / sluggish / pushed against while walking". Player-controlled pawns keep the
	// engine rates; the throttle still lands on future crowd/NPC hub actors. Quantisation and
	// the zone-cull machinery below stay for everyone (they do not fight the owner's CMC).
	// POSSESSION-ROBUST: BeginPlay usually precedes possession, so IsPlayerControlled() here
	// would misread the player as an NPC and throttle them anyway. For pawns the frequency
	// decision is DEFERRED 2s (twice, in case of a slow join); everything else decides now.
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	bool bPlayerControlled = OwnerPawn && OwnerPawn->IsPlayerControlled();
	if (OwnerPawn && !bPlayerControlled)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(FreqDecisionTimer, this,
				&UAFLHubNetProfileComponent::DecideFrequencyThrottle, 2.0f, /*bLoop*/ true);
		}
	}
	else if (!bPlayerControlled)
	{
		Owner->SetNetUpdateFrequency(HubNetUpdateFrequency);
		Owner->SetMinNetUpdateFrequency(HubMinNetUpdateFrequency);
	}

	FRepMovement& RepMove = Owner->GetReplicatedMovement_Mutable();
	CachedLocationQuantization = static_cast<uint8>(RepMove.LocationQuantizationLevel);
	CachedRotationQuantization = static_cast<uint8>(RepMove.RotationQuantizationLevel);
	CachedVelocityQuantization = static_cast<uint8>(RepMove.VelocityQuantizationLevel);
	RepMove.LocationQuantizationLevel = EVectorQuantization::RoundOneDecimal;
	RepMove.RotationQuantizationLevel = ERotatorQuantization::ByteComponents;
	RepMove.VelocityQuantizationLevel = EVectorQuantization::RoundWholeNumber;

	bPostureApplied = true;
	UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBNET: %s posture applied (%s; quantised; was %.0f/%.0f)."),
		*GetNameSafe(Owner),
		bPlayerControlled ? TEXT("PLAYER-CONTROLLED -> engine freq kept") :
			*FString::Printf(TEXT("freq %.0f/%.0f Hz"), HubNetUpdateFrequency, HubMinNetUpdateFrequency),
		CachedNetUpdateFrequency, CachedMinNetUpdateFrequency);
}

void UAFLHubNetProfileComponent::DecideFrequencyThrottle()
{
	AActor* Owner = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!Owner || !OwnerPawn)
	{
		if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(FreqDecisionTimer); }
		return;
	}
	FreqDecisionPolls++;
	const bool bPlayerControlled = OwnerPawn->IsPlayerControlled();
	if (bPlayerControlled || FreqDecisionPolls >= 3)
	{
		if (!bPlayerControlled)
		{
			Owner->SetNetUpdateFrequency(HubNetUpdateFrequency);
			Owner->SetMinNetUpdateFrequency(HubMinNetUpdateFrequency);
			UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBNET: %s NPC/crowd -> throttled %.0f/%.0f Hz."),
				*GetNameSafe(Owner), HubNetUpdateFrequency, HubMinNetUpdateFrequency);
		}
		else
		{
			UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBNET: %s PLAYER-CONTROLLED -> engine freq kept (AAA movement ruling)."),
				*GetNameSafe(Owner));
		}
		if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(FreqDecisionTimer); }
	}
}

void UAFLHubNetProfileComponent::RestoreNetPosture()
{
	AActor* Owner = GetOwner();
	if (!Owner || !bPostureApplied)
	{
		return;
	}

	Owner->SetNetUpdateFrequency(CachedNetUpdateFrequency);
	Owner->SetMinNetUpdateFrequency(CachedMinNetUpdateFrequency);
	Owner->SetNetCullDistanceSquared(CachedNetCullDistanceSquared);

	FRepMovement& RepMove = Owner->GetReplicatedMovement_Mutable();
	RepMove.LocationQuantizationLevel = static_cast<EVectorQuantization>(CachedLocationQuantization);
	RepMove.RotationQuantizationLevel = static_cast<ERotatorQuantization>(CachedRotationQuantization);
	RepMove.VelocityQuantizationLevel = static_cast<EVectorQuantization>(CachedVelocityQuantization);

	bPostureApplied = false;
}

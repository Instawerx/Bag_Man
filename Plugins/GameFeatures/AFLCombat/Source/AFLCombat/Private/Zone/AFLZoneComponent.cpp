// Copyright C12 AI Gaming. All Rights Reserved.

#include "Zone/AFLZoneComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AFLCombat.h"                                   // LogAFLCombat
#include "BattleRoyale/AFLBattleRoyaleComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Telemetry/AFLCombatTelemetry.h"
#include "Zone/AFLZoneConfig.h"

// Declared in AFLCombatTags.ini. Applied to a pawn's ASC while it stands outside the circle -- the client's
// signal for the damage vignette and the alarm loop. The zone's authority is the replicated state; this tag
// is a convenience for presentation, never a gate on damage.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Zone_Outside, "State.Zone.Outside");
// SetByCaller channel the outside-damage GE reads its magnitude from. Shared with the rest of the damage
// stack, so GE_AFL_Zone_DoT is an ordinary member of it rather than a special case.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Damage_Zone, "Data.Damage");

namespace
{
	/** The zone's rules with no config asset assigned. Playable, and loudly flagged at BeginPlay. */
	const FAFLZoneRules& FallbackRules()
	{
		static const FAFLZoneRules Defaults;
		return Defaults;
	}
}

UAFLZoneComponent::UAFLZoneComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Throttled, exactly like the round manager. The zone clock is a countdown a player reads in whole
	// seconds; resolving it per frame would buy nothing and cost a tick on the GameState every frame of a
	// 36-player match.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.25f;

	SetIsReplicatedByDefault(true);
}

void UAFLZoneComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLZoneComponent, CurrentCentre);
	DOREPLIFETIME(UAFLZoneComponent, CurrentRadius);
	DOREPLIFETIME(UAFLZoneComponent, TargetCentre);
	DOREPLIFETIME(UAFLZoneComponent, TargetRadius);
	DOREPLIFETIME(UAFLZoneComponent, PhaseIndex);
	DOREPLIFETIME(UAFLZoneComponent, ZoneState);
	DOREPLIFETIME(UAFLZoneComponent, TimeToNextEvent);
	DOREPLIFETIME(UAFLZoneComponent, PlanSeed);
}

bool UAFLZoneComponent::HasAuth() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

void UAFLZoneComponent::BeginPlay()
{
	Super::BeginPlay();

	// Clients consume the replicated state through OnRep and never compute anything -- §4's staking line.
	if (!GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (!Config)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_ZONE: no UAFLZoneConfig assigned -- falling back to FAFLZoneRules defaults, which centre the "
			     "zone on WORLD ORIGIN. That is almost certainly not where the map is. Assign a config on the "
			     "experience's AddComponents row."));
	}
	else if (!Config->OutsideDamageEffect)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_ZONE: config '%s' has no OutsideDamageEffect -- the zone will shrink and telegraph but "
			     "will NOT damage anyone outside it. Assign GE_AFL_Zone_DoT."), *Config->GetName());
	}

	// Deliberately NOT observing AFL.GamePhase.Playing. The BR component observes the same transition to
	// author MatchId, and two components racing on one phase event is a coin flip over whether the seed
	// exists yet. Polling for the id instead makes the dependency explicit and the ordering irrelevant.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ZONE: armed -- waiting for a MatchId from the BR component to seed the plan."));
}

void UAFLZoneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Drop the outside tag from anyone still carrying it. The damage itself is instant-per-period and leaves
	// nothing behind, but this tag is stateful and a match ending while someone is outside would strand it.
	for (const TWeakObjectPtr<APawn>& Weak : TaggedOutside)
	{
		if (APawn* Pawn = Weak.Get())
		{
			SetOutsideTag(Pawn, false);
		}
	}
	TaggedOutside.Reset();

	Super::EndPlay(EndPlayReason);
}

const FAFLZoneRules& UAFLZoneComponent::EffectiveRules() const
{
	return Config ? Config->Rules : FallbackRules();
}

float UAFLZoneComponent::EffectiveDamagePeriod() const
{
	return Config ? FMath::Max(0.1f, Config->DamagePeriodSeconds) : 1.f;
}

UAFLBattleRoyaleComponent* UAFLZoneComponent::ResolveBattleRoyale()
{
	if (BattleRoyale.IsValid())
	{
		return BattleRoyale.Get();
	}
	if (AActor* Owner = GetOwner())
	{
		UAFLBattleRoyaleComponent* Found = Owner->FindComponentByClass<UAFLBattleRoyaleComponent>();
		BattleRoyale = Found;
		return Found;
	}
	return nullptr;
}

void UAFLZoneComponent::ServerStartZone(const FGuid& MatchId)
{
	if (!HasAuth() || bStarted)
	{
		return;
	}

	Plan = FAFLZonePlan::Build(FAFLZonePlan::SeedFromGuid(MatchId), EffectiveRules());
	if (!Plan.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_ZONE: plan build produced %d phases -- refusing to start."), Plan.Phases.Num());
		return;
	}

	bStarted = true;
	PlanSeed = Plan.Seed;

	// THE DISPUTE RECORD. The full sequence is logged once, at start, before anything has happened -- so a
	// contested staked match is settled by comparing this block against a rebuild from the same MatchId,
	// rather than by reconstructing intent from a scatter of per-phase lines.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ZONE_PLAN match=%s %s"),
		*MatchId.ToString(EGuidFormats::DigitsWithHyphens), *Plan.ToLogString());

	FAFLCombatTelemetry::EmitZonePlan(MatchId.ToString(EGuidFormats::DigitsWithHyphens), Plan.Seed, Plan.Phases.Num());

	EnterPhase(0);
}

void UAFLZoneComponent::EnterPhase(int32 Index)
{
	if (!Plan.Phases.IsValidIndex(Index))
	{
		return;
	}

	const FAFLZonePhasePlan& Now = Plan.Phases[Index];
	PhaseIndex = Index;
	PhaseElapsed = 0.f;

	CurrentCentre = FVector(Now.Centre.X, Now.Centre.Y, 0.f);
	CurrentRadius = Now.Radius;

	if (Plan.Phases.IsValidIndex(Index + 1))
	{
		// TELEGRAPH. The next circle is published the instant this one becomes current -- i.e. for the whole
		// hold, before any movement. §4 calls this tournament-critical, and it is the difference between a
		// shrink you lost to and a shrink you were beaten by.
		const FAFLZonePhasePlan& Next = Plan.Phases[Index + 1];
		TargetCentre = FVector(Next.Centre.X, Next.Centre.Y, 0.f);
		TargetRadius = Next.Radius;
		ZoneState = (Next.HoldSeconds > 0.f) ? EAFLSafeZoneState::Holding : EAFLSafeZoneState::Shrinking;
		TimeToNextEvent = (Next.HoldSeconds > 0.f) ? Next.HoldSeconds : Next.ShrinkSeconds;
	}
	else
	{
		TargetCentre = CurrentCentre;
		TargetRadius = CurrentRadius;
		ZoneState = EAFLSafeZoneState::Final;
		TimeToNextEvent = 0.f;
	}

	FAFLCombatTelemetry::EmitZonePhase(PhaseIndex, Now.Centre, Now.Radius, CurrentDamagePerSecond());

	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_ZONE phase=%d state=%s centre=(%.0f,%.0f) r=%.0f -> next centre=(%.0f,%.0f) r=%.0f in %.1fs"),
		PhaseIndex, *UEnum::GetValueAsString(ZoneState),
		CurrentCentre.X, CurrentCentre.Y, CurrentRadius,
		TargetCentre.X, TargetCentre.Y, TargetRadius, TimeToNextEvent);

	OnRep_ZoneState();      // listen-host: drive the local delegate the same way a client gets it
}

void UAFLZoneComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasAuth())
	{
		return;
	}

	if (bStopped)
	{
		return;
	}

	if (!bStarted)
	{
		// Poll for the seed. Cheap (4 Hz, one cached pointer deref) and it removes the ordering race with
		// the BR component entirely -- see the class comment.
		UAFLBattleRoyaleComponent* BR = ResolveBattleRoyale();
		if (!BR)
		{
			if (!bLoggedWaitingForMatchId)
			{
				bLoggedWaitingForMatchId = true;
				UE_LOG(LogAFLCombat, Warning,
					TEXT("AFL_ZONE: no UAFLBattleRoyaleComponent on the GameState -- the zone is BR-only (scope §1) "
					     "and will stay idle. If this is a district experience (Duel/Arena/Team) that is correct; "
					     "if it is a BR experience, the AddComponents row is missing the BR component."));
			}
			return;
		}
		if (BR->MatchId.IsValid())
		{
			ServerStartZone(BR->MatchId);
		}
		return;
	}

	// ══ THE MATCH IS THE ZONE'S LIFETIME ═══════════════════════════════════════════════════════════════
	//
	// Found in PIE, and it is the whole reason this check exists: the zone used to run from MatchId until
	// EndPlay, with nothing tying it to the match it belongs to. At last-standing the BR component restores
	// respawn -- correct, post-game players should be able to spawn and move around -- and the ring was
	// still shrinking and still lethal. So every player respawned into it, died, respawned, died. It never
	// converged: one PIE session reached pawn index ~9,700 and exhausted the editor's memory.
	//
	// The bug was invisible in the plan, in the tests, and in the first sixty seconds of the run. It only
	// appears AFTER the win condition resolves, which is exactly the moment nobody is watching the zone.
	UAFLBattleRoyaleComponent* BR = ResolveBattleRoyale();
	if (!BR || !BR->IsMatchActive())
	{
		ServerStopZone(BR ? TEXT("match resolved") : TEXT("BR component gone"));
		return;
	}

	AdvancePlan(DeltaTime);

	DamageAccum += DeltaTime;
	const float Period = EffectiveDamagePeriod();
	if (DamageAccum >= Period)
	{
		ApplyOutsideDamage(DamageAccum);
		DamageAccum = 0.f;
	}
}

void UAFLZoneComponent::ServerStopZone(const TCHAR* Reason)
{
	if (bStopped)
	{
		return;
	}
	bStopped = true;

	// Idle means "nowhere is outside" (see IsInsideZone), so the damage path is closed by the same flag the
	// HUD reads to hide the ring -- one state change, not two that could disagree.
	ZoneState = EAFLSafeZoneState::Idle;
	TimeToNextEvent = 0.f;

	// Clear the outside tag from anyone still carrying it. The damage is instant-per-period and leaves
	// nothing behind, but this tag is stateful and would otherwise strand a vignette on a post-game screen.
	for (const TWeakObjectPtr<APawn>& Weak : TaggedOutside)
	{
		if (APawn* Pawn = Weak.Get())
		{
			SetOutsideTag(Pawn, false);
		}
	}
	TaggedOutside.Reset();

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ZONE: stopped at phase %d -- %s. Ring is inert; no further damage."),
		PhaseIndex, Reason);

	OnRep_ZoneState();
}

void UAFLZoneComponent::AdvancePlan(float DeltaTime)
{
	if (ZoneState == EAFLSafeZoneState::Final || ZoneState == EAFLSafeZoneState::Idle)
	{
		return;
	}

	const int32 NextIndex = PhaseIndex + 1;
	if (!Plan.Phases.IsValidIndex(NextIndex))
	{
		ZoneState = EAFLSafeZoneState::Final;
		OnRep_ZoneState();
		return;
	}
	const FAFLZonePhasePlan& Next = Plan.Phases[NextIndex];

	PhaseElapsed += DeltaTime;

	if (ZoneState == EAFLSafeZoneState::Holding)
	{
		const float Remaining = Next.HoldSeconds - PhaseElapsed;
		if (Remaining > 0.f)
		{
			TimeToNextEvent = Remaining;
			return;
		}
		// Hold expired: carry the overshoot into the shrink so a 0.25s tick cannot stretch the sequence.
		PhaseElapsed = -Remaining;
		ZoneState = EAFLSafeZoneState::Shrinking;
		OnRep_ZoneState();
	}

	if (ZoneState == EAFLSafeZoneState::Shrinking)
	{
		const float Duration = FMath::Max(Next.ShrinkSeconds, KINDA_SMALL_NUMBER);
		const float Alpha = FMath::Clamp(PhaseElapsed / Duration, 0.f, 1.f);

		// Interpolate BOTH centre and radius. Moving the radius alone would slide the far edge across
		// players standing still near the old boundary without the near edge ever appearing to move.
		const FAFLZonePhasePlan& From = Plan.Phases[PhaseIndex];
		CurrentCentre = FMath::Lerp(FVector(From.Centre.X, From.Centre.Y, 0.f), TargetCentre, Alpha);
		CurrentRadius = FMath::Lerp(From.Radius, Next.Radius, Alpha);
		TimeToNextEvent = Duration * (1.f - Alpha);

		if (Alpha >= 1.f)
		{
			EnterPhase(NextIndex);
		}
	}
}

bool UAFLZoneComponent::IsInsideZone(const FVector& WorldLocation) const
{
	if (ZoneState == EAFLSafeZoneState::Idle || CurrentRadius <= 0.f)
	{
		return true;      // no zone yet means nowhere is outside it
	}
	const FVector2D Delta(WorldLocation.X - CurrentCentre.X, WorldLocation.Y - CurrentCentre.Y);
	return Delta.SizeSquared() <= (CurrentRadius * CurrentRadius);
}

float UAFLZoneComponent::CurrentDamagePerSecond() const
{
	// Pressure belongs to the circle being shrunk TOWARD -- the one whose deadline the player is racing. At
	// the final circle there is no next, so the last phase's own figure stands.
	const int32 Index = Plan.Phases.IsValidIndex(PhaseIndex + 1) ? PhaseIndex + 1 : PhaseIndex;
	return Plan.Phases.IsValidIndex(Index) ? Plan.Phases[Index].DamagePerSecond : 0.f;
}

void UAFLZoneComponent::ApplyOutsideDamage(float Period)
{
	const AGameStateBase* GS = GetGameState<AGameStateBase>();
	if (!GS || ZoneState == EAFLSafeZoneState::Idle)
	{
		return;
	}

	const float DPS = CurrentDamagePerSecond();
	const TSubclassOf<UGameplayEffect> EffectClass = Config ? Config->OutsideDamageEffect : nullptr;

	for (APlayerState* PS : GS->PlayerArray)
	{
		APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		const bool bOutside = !IsInsideZone(Pawn->GetActorLocation());
		SetOutsideTag(Pawn, bOutside);

		if (!bOutside || DPS <= 0.f || !EffectClass)
		{
			continue;
		}

		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		if (!ASC)
		{
			continue;
		}

		// SELF-APPLIED AND INSTANT. Self-applied because the zone is not a shooter -- routing it through a
		// source ASC would attribute the kill to somebody and put it through team filters that have nothing
		// to say about weather. Instant because an effect that does not persist cannot leak: re-entry,
		// death, disconnect and re-possession all need zero teardown, which is why Z3 needs no cleanup path.
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage_Zone, DPS * Period);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
}

void UAFLZoneComponent::SetOutsideTag(APawn* Pawn, bool bOutside)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC)
	{
		return;
	}

	const TWeakObjectPtr<APawn> Key(Pawn);
	const bool bTagged = TaggedOutside.Contains(Key);

	if (bOutside && !bTagged)
	{
		ASC->AddLooseGameplayTag(TAG_State_Zone_Outside);
		TaggedOutside.Add(Key);
	}
	else if (!bOutside && bTagged)
	{
		ASC->RemoveLooseGameplayTag(TAG_State_Zone_Outside);
		TaggedOutside.Remove(Key);
	}
}

void UAFLZoneComponent::OnRep_ZoneState()
{
	OnZoneStateChanged.Broadcast();
}

void UAFLZoneComponent::LogBeliefState(const FString& Context) const
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_ZONE_STATE [%s] started=%d state=%s phase=%d/%d seed=%d centre=(%.0f,%.0f) r=%.0f "
		     "target=(%.0f,%.0f) r=%.0f next=%.1fs dps=%.1f"),
		*Context, bStarted ? 1 : 0, *UEnum::GetValueAsString(ZoneState),
		PhaseIndex, FMath::Max(0, Plan.Phases.Num() - 1), PlanSeed,
		CurrentCentre.X, CurrentCentre.Y, CurrentRadius,
		TargetCentre.X, TargetCentre.Y, TargetRadius, TimeToNextEvent, CurrentDamagePerSecond());

	if (const AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_ZONE_STATE   %s | pawn=%d | inside=%d"),
				PS ? *PS->GetPlayerName() : TEXT("<null>"),
				Pawn ? 1 : 0,
				Pawn ? (IsInsideZone(Pawn->GetActorLocation()) ? 1 : 0) : -1);
		}
	}
#endif
}

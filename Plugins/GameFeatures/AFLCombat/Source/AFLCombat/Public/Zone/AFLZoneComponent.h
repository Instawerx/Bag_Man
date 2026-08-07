// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"
#include "Zone/AFLZonePlan.h"

#include "AFLZoneComponent.generated.h"

class APawn;
class APlayerState;
class UAFLZoneConfig;
class UAFLBattleRoyaleComponent;

/** What the zone is doing right now (replicated to the HUD via OnRep_ZoneState).
 *  SAFE-zone, not EXTRACTION-zone: `EAFLZoneState` is already taken by AFLExtractionZone.h, and UHT
 *  enforces globally-unique enum names regardless of C++ namespace. */
UENUM(BlueprintType)
enum class EAFLSafeZoneState : uint8
{
	/** No plan yet — waiting on the MatchId that seeds it. Nothing is dangerous. */
	Idle,
	/** Resting on the current circle. The NEXT circle is already published — this is the telegraph window. */
	Holding,
	/** Interpolating from the current circle to the next. */
	Shrinking,
	/** Final circle reached; it no longer moves. Damage outside continues. */
	Final
};

/**
 * UAFLZoneComponent — the shrinking safe zone (IRONICS_BR_ZONE_SYSTEM.md §3.1).
 *
 * A server-authoritative GameState component that forces engagement by shrinking a safe radius and
 * damaging anyone outside it. The single genuinely net-new BR system; everything else in the ruleset is
 * shipped or reused (BR_MODE_SPIKE §4).
 *
 * PROVEN-SIBLING BASIS: UAFLRoundManagerComponent. Same shape throughout — a UGameStateComponent arriving
 * via the experience AddComponents row, `HasAuthority()` gate in BeginPlay, throttled server tick,
 * plain replicated UPROPERTYs read by the HUD through OnRep. Net safety follows the round manager's rule:
 * NO custom net-serialized struct.
 *
 * ══ THE STAKING LINE ══════════════════════════════════════════════════════════════════════════════════
 *
 * §4: zero client input to zone placement. The server computes the plan, the state, and the damage;
 * clients render replicated values and nothing else. Two consequences worth stating because they look
 * like defects until you know why:
 *
 *   1. NO MATCHID, NO ZONE. The plan is seeded from UAFLBattleRoyaleComponent::MatchId, so the zone does
 *      not start until that id exists. It waits rather than inventing a seed, because a zone seeded from
 *      anything else is a zone a dispute cannot replay — and an unreplayable staked match is worse than
 *      a late one. The wait is a poll on the existing throttled tick, which also sidesteps the ordering
 *      race between two components observing the same AFL.GamePhase.Playing transition.
 *
 *   2. THE WHOLE PLAN IS BUILT AND LOGGED AT START. Not phase by phase. See FAFLZonePlan for why lazy
 *      draws make determinism a property of call order, and therefore not a property at all.
 *
 * SCOPE: full-map BR only (§1). The district modes — Duel / Arena / Team, ShantyTown §11 — are fixed
 * fenced arenas with NO ring, so this component is added to the BR experience and to nothing else. It
 * additionally refuses to run without a BR sibling, so a mis-authored AddComponents row is a log line
 * rather than a zone slowly killing everyone in a 1v1.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLZoneComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UAFLZoneComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Geometry, pacing, damage curve, and the outside-damage effect. Null = FAFLZoneRules defaults + a loud log. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Zone") TObjectPtr<UAFLZoneConfig> Config;

	// -- replicated state (the HUD's entire source of truth) --

	/** Centre of the circle that is dangerous to be outside of RIGHT NOW. Z is unused — the test is a cylinder. */
	UPROPERTY(ReplicatedUsing = OnRep_ZoneState, BlueprintReadOnly, Category = "AFL|Zone") FVector CurrentCentre = FVector::ZeroVector;
	UPROPERTY(ReplicatedUsing = OnRep_ZoneState, BlueprintReadOnly, Category = "AFL|Zone") float CurrentRadius = 0.f;

	/** The circle being shrunk toward — published DURING the hold, which is what makes the shrink fair. */
	UPROPERTY(ReplicatedUsing = OnRep_ZoneState, BlueprintReadOnly, Category = "AFL|Zone") FVector TargetCentre = FVector::ZeroVector;
	UPROPERTY(ReplicatedUsing = OnRep_ZoneState, BlueprintReadOnly, Category = "AFL|Zone") float TargetRadius = 0.f;

	/** Index into the plan of the circle currently being held at / shrunk from. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|Zone") int32 PhaseIndex = 0;
	UPROPERTY(ReplicatedUsing = OnRep_ZoneState, BlueprintReadOnly, Category = "AFL|Zone") EAFLSafeZoneState ZoneState = EAFLSafeZoneState::Idle;

	/** Seconds until the next shrink STARTS (Holding), or until the current one COMPLETES (Shrinking). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|Zone") float TimeToNextEvent = 0.f;

	/** The seed the plan was built from. Replicated so a client-side observer tool can rebuild the sequence. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|Zone") int32 PlanSeed = 0;

	/** Fires server-side on every phase transition, and on clients via OnRep_ZoneState. HUD/audio bind. */
	DECLARE_MULTICAST_DELEGATE(FAFLZoneStateChanged);
	FAFLZoneStateChanged OnZoneStateChanged;

	UFUNCTION(BlueprintPure, Category = "AFL|Zone") bool IsZoneActive() const { return ZoneState != EAFLSafeZoneState::Idle; }

	/** Is a world location inside the currently-dangerous circle? Cylinder test — height never gates it,
	 *  because a player on a roof inside the circle is inside the circle, and ShantyTown has roofs. */
	UFUNCTION(BlueprintPure, Category = "AFL|Zone") bool IsInsideZone(const FVector& WorldLocation) const;

	/** The built plan (server-side; empty on clients). Exposed for the test runner and the dispute dump. */
	const FAFLZonePlan& GetPlan() const { return Plan; }

	/** Build the plan and begin. Authority; idempotent. Normally driven by the MatchId poll; the
	 *  `afl.Zone.Start` cheat and the headless runner call it directly. */
	void ServerStartZone(const FGuid& MatchId);

	/** Dev-only full-state dump, mirroring UAFLBattleRoyaleComponent::LogBeliefState — so a zone that looks
	 *  wrong in PIE is a READ rather than an inference. Pure logging; changes nothing. */
	void LogBeliefState(const FString& Context) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION() void OnRep_ZoneState();

private:
	bool HasAuth() const;

	/** Resolve (and cache) the BR sibling on the GameState. The zone's MatchId source and its gate. */
	UAFLBattleRoyaleComponent* ResolveBattleRoyale();

	/** Advance the clock through hold -> shrink -> hold. Server only. */
	void AdvancePlan(float DeltaTime);
	/** Publish the circle for `Index` as CURRENT, and the one after it as TARGET. */
	void EnterPhase(int32 Index);
	/** Apply one period of damage to every living participant outside the current circle. Server only. */
	void ApplyOutsideDamage(float Period);
	/** Add/remove State.Zone.Outside on a pawn's ASC so clients get a signal for HUD and audio. */
	void SetOutsideTag(APawn* Pawn, bool bOutside);

	const FAFLZoneRules& EffectiveRules() const;
	float EffectiveDamagePeriod() const;
	/** DPS of the circle currently being shrunk toward — outside pressure ramps with the phase. */
	float CurrentDamagePerSecond() const;

	FAFLZonePlan Plan;

	/** Seconds elapsed inside the current hold-or-shrink leg. */
	float PhaseElapsed = 0.f;
	/** Accumulator for the damage period, independent of the tick interval. */
	float DamageAccum = 0.f;

	bool bStarted = false;
	/** Logged once when the MatchId poll first fails, so a stuck zone says why exactly once. */
	bool bLoggedWaitingForMatchId = false;

	TWeakObjectPtr<UAFLBattleRoyaleComponent> BattleRoyale;
	/** Pawns currently carrying State.Zone.Outside, so the tag is cleared exactly once on re-entry. */
	TSet<TWeakObjectPtr<APawn>> TaggedOutside;
};

// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/TimerHandle.h"

#include "AFLHubNetProfileComponent.generated.h"

class UAbilitySystemComponent;
class UAFLHubZoneProfiles;

/**
 * UAFLHubNetProfileComponent  (AFL-3012 / H1 -- hub net posture on the hero pawn)
 *
 * Delivers the hub's 10-15 Hz / quantised / zone-culled replication targets (SSOT s2.2) with ENGINE
 * facilities on the EXISTING Lyra hero pawn -- no custom net struct, no pawn subclass, no CMC touch.
 * P-CONTROLS doctrine: a GameFeatureAction_AddComponents component, structurally UAFLSprintMovementComponent
 * (the proven sibling) with these deliberate divergences:
 *  - What the tag listener DRIVES: sprint swaps CMC floats; this swaps AActor net knobs
 *    (NetUpdateFrequency / NetCullDistanceSquared / FRepMovement quantisation). Nothing else moves.
 *  - MANY tags, not one: a listener per DA_AFL_HubZoneProfiles row (RegisterGameplayTagEvent per zone
 *    tag, per the AC), where sprint binds a single state tag.
 *  - No re-entrancy cache dance: zone cull is RECOMPUTED from live ASC tag state on every change
 *    (idempotent by construction), where sprint must cache-and-restore a mutable float.
 *
 * Overlapping zones: the pawn can hold two Hub.Zone.* tags at once (volumes may intersect at seams).
 * Rule: the SMALLEST active cull distance wins -- the denser zone's perf posture is the safe one --
 * and DefaultNetCullDistance (DA) applies when no profiled zone tag is present.
 *
 * Blueprintable is LOAD-BEARING (the Sprint/Holster precedent verbatim): AddComponents stores a CLASS,
 * so the ZoneProfiles asset pointer below can only be wired on a BP child's CDO
 * (BPC_AFL_HubNetProfile with ZoneProfiles = DA_AFL_HubZoneProfiles). A C++ CDO cannot reference the
 * DA without a hardcoded path.
 */
UCLASS(ClassGroup = (AFL), Blueprintable, meta = (BlueprintSpawnableComponent))
class AFLHUB_API UAFLHubNetProfileComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLHubNetProfileComponent();

	/** Hub pawn net update ceiling, Hz (match pawns run the Lyra default 100). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Hub|Net", meta = (ClampMin = "1.0"))
	float HubNetUpdateFrequency = 15.0f;

	/** Adaptive-frequency floor, Hz -- an idle hub pawn decays to this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Hub|Net", meta = (ClampMin = "1.0"))
	float HubMinNetUpdateFrequency = 5.0f;

	/** The per-zone tuning table (rows = cull distance + mirror capture; AFL-3012 AC). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Hub|Net")
	TObjectPtr<const UAFLHubZoneProfiles> ZoneProfiles;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Deferred-player ASC-ready callback (PawnExtension hook; Sprint pattern). */
	void OnAbilitySystemReady();

	void BindToAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindFromAbilitySystem();

	/** Any profiled zone tag rose or fell -> recompute the cull posture from live ASC state. */
	void HandleZoneTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Deferred pawn frequency decision -- possession lands after BeginPlay (AAA movement ruling). */
	void DecideFrequencyThrottle();
	FTimerHandle FreqDecisionTimer;
	int32 FreqDecisionPolls = 0;

	/** Smallest active zone cull distance, else the DA default; applied to the owner. */
	void RecomputeCullDistance();

	/** Apply / restore the frequency + quantisation posture on the owner actor. */
	void ApplyNetPosture();
	void RestoreNetPosture();

	/** Owner values cached at apply, restored at EndPlay (GameFeature deactivation mid-session). */
	float CachedNetUpdateFrequency = 0.0f;
	float CachedMinNetUpdateFrequency = 0.0f;
	float CachedNetCullDistanceSquared = 0.0f;
	uint8 CachedLocationQuantization = 0;
	uint8 CachedRotationQuantization = 0;
	uint8 CachedVelocityQuantization = 0;
	bool bPostureApplied = false;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	/** One handle per profiled zone tag, for exact unbind. */
	TArray<TPair<FGameplayTag, FDelegateHandle>> ZoneTagHandles;
};

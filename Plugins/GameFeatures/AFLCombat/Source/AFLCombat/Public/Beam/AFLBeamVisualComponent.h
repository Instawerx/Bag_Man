// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "AFLBeamVisualComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UAFLBeamChannelComponent;


/**
 * UAFLBeamVisualComponent  (CANARY — clean-room beam delivery, Q2=(a))
 *
 * The rebuild's beam-visual model, REPLACING the cue-spawn-per-fire path that was fragile all
 * session. Lives on the WEAPON DISPLAY ACTOR (which we proved replicates to proxies, commit
 * 1c3cb0f9 / bReplicates=true). Carries ONE persistent UNiagaraComponent (Auto-Activate OFF) and
 * TOGGLES it Activate/Deactivate — it never spawns/destroys per fire (can't stick, can't stack).
 *
 * WHY ON THE WEAPON ACTOR (Q2=a): the cue->proxy delivery path was never root-caused last session.
 * Routing the visual through the weapon actor's PROVEN replication channel stops building on the
 * unsolved cue path. See Tools/FIRING_SYSTEM_REBUILD_PLAN.md.
 *
 * TOGGLE REPLICATION (Edge 1 — the load-bearing design):
 *   - A replicated bool `bBeamActive` with OnRep_bBeamActive.
 *   - SetBeamActive(bool) is AUTHORITY-only: sets the value AND calls ApplyBeamActiveState()
 *     LOCALLY (so the listen-host-as-player toggles -- OnRep does NOT fire on the server).
 *   - OnRep_bBeamActive() calls the SAME ApplyBeamActiveState() (so simulated proxies toggle).
 *   - Result: host-as-player, owning client, AND proxy all run identical toggle logic. The classic
 *     OnRep-only "works on client, not host" bug is designed out.
 *
 * ENDPOINT (Edge 2 — cadence, not framerate): the beam endpoint + muzzle come from the pawn's
 * UAFLBeamChannelComponent (the doctrine-trace published-value bridge, Q1=b). This component READS
 * those replicated FVector_NetQuantize values each tick to drive the NS User."Beam End" / start.
 * They replicate at NetUpdateFrequency (~10-30 Hz), NOT per-frame -- endpoint lag/step on a proxy
 * is EXPECTED and tunable, NOT a toggle failure. Do not conflate.
 *
 * Doctrine boundary held: gameplay (ability) owns the trace + publishes the world point; this
 * cosmetic component only READS it + drives Niagara. No attributes here, no trace here.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLBeamVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLBeamVisualComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * AUTHORITY-ONLY toggle entry point. Sets the replicated bBeamActive AND applies the visual
	 * state locally (server-as-player path -- OnRep won't fire on the server). The ability calls
	 * this on channel start (true) / end (false).
	 */
	void SetBeamActive(bool bInActive);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** OnRep for remote clients -- calls the SAME ApplyBeamActiveState as the server path (Edge 1). */
	UFUNCTION()
	void OnRep_bBeamActive();

	/**
	 * The ONE place the visual toggles. Both the server (in SetBeamActive) and the remote-client
	 * OnRep call this -- identical logic on every machine. Spawns the persistent NS on first use
	 * (Auto-Activate OFF), then Activate()s / Deactivate()s it; never destroys per fire.
	 */
	void ApplyBeamActiveState(bool bActive);

	/** Find the firing pawn's UAFLBeamChannelComponent (the published-endpoint bridge, Q1=b). */
	UAFLBeamChannelComponent* ResolveChannel() const;

	/**
	 * THE UNIFIED FX TINT INPUT. Reads the weapon's IAFLLaserVisualProvider::GetBeamColor -- the SAME
	 * per-weapon tint the pulse Fire/Tracer cues drive User.Color from. Walks owner-actor -> pawn ->
	 * equipment manager -> the instance that spawned us, and reads its GetBeamColor. The colour is
	 * editor-authored data (identical on every machine), so it resolves locally with no replication.
	 * Returns A<=0 when no provider tint is set -> the caller falls back to the deprecated
	 * BeamColorOverride during beam-weapon migration. ONE tint input for beams + pulses; the cosmetic
	 * resolver writes only GetBeamColor.
	 */
	FLinearColor ResolveProviderTint() const;

	/** The beam NS to play. Set on the canary weapon (reuse NS_AFL_Laser_Twist). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Beam")
	TObjectPtr<UNiagaraSystem> BeamSystem = nullptr;

	/** Niagara User param names (match the imported marketplace contract). */
	UPROPERTY(EditAnywhere, Category = "AFL|Beam")
	FName BeamEndParam = FName("Beam End");

	UPROPERTY(EditAnywhere, Category = "AFL|Beam")
	FName ColorParam = FName("User.Color");

	/**
	 * DEPRECATED legacy tint input -- superseded by the unified IAFLLaserVisualProvider::GetBeamColor
	 * (see ResolveProviderTint). Read ONLY as a migration fallback while a beam weapon still lacks a
	 * GetBeamColor override; remove once every beam weapon drives GetBeamColor. Do NOT add new uses.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Beam")
	FLinearColor BeamColorOverride = FLinearColor(0, 0, 0, 0);

private:
	/** Replicated toggle. OnRep drives remote clients; the server applies locally in SetBeamActive. */
	UPROPERTY(ReplicatedUsing = OnRep_bBeamActive, Transient)
	bool bBeamActive = false;

	/** The persistent beam NS instance (Auto-Activate OFF). Spawned once, toggled -- never destroyed per fire. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BeamNC = nullptr;

	/** Cached pawn beam-channel bridge (re-resolved if stale). */
	UPROPERTY(Transient)
	TWeakObjectPtr<UAFLBeamChannelComponent> CachedChannel;
};

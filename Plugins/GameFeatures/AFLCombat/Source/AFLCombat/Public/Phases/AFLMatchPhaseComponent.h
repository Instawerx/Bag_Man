// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"
#include "GameplayTagContainer.h"

#include "AFLMatchPhaseComponent.generated.h"

class ULyraGamePhaseAbility;
class ULyraGamePhaseSubsystem;

/**
 * UAFLMatchPhaseComponent  (match phases cycle 1 -- the driver, AFL-0902/0804)
 *
 * Server-only GameStateComponent (the Lyra scoring-component shape -- arrives via the experience
 * AddComponents row like LyraTeamCreationComponent / B_*Scoring). It is the SINGLE C++ owner of the
 * match phase clock.
 *
 * ARCHITECTURE NOTE (the Lyra export boundary): ULyraGamePhaseAbility is NOT LYRAGAME_API-exported,
 * so a GameFeature module CANNOT subclass it in C++ (link error on the ctor/vtable). ShooterCore's
 * Phase_* are BLUEPRINT children for exactly this reason. So our two phases are BP shells (just a
 * GamePhaseTag), and ALL the timing/announce/force-close logic that would have lived in a C++ window
 * phase lives HERE instead -- the driver is a C++ module class and links fine, and it reaches the
 * (non-exported) subsystem only through its UFUNCTION surface (K2_StartPhase / IsPhaseActive).
 *
 * BeginPlay(authority): K2_StartPhase(PlayingPhaseClass) [never-ending shell] -> cadence loop opens
 * a window every afl.Extract.WindowPeriod (read PER SCHEDULE). Each window: K2_StartPhase(Window) +
 * dual-broadcast WindowOpen + arm a WindowDuration timer that force-closes it (cancel by class) +
 * dual-broadcast WindowClosed. Re-entrancy guard (IsPhaseActive). afl.Extract.ForceWindow open|close.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLMatchPhaseComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UAFLMatchPhaseComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Cheat entry points (afl.Extract.ForceWindow). Authority-only; no-op otherwise. */
	void ForceWindowOpen();
	void ForceWindowClose();

	/** Re-arm the cadence timer NOW (reads afl.Extract.WindowPeriod fresh). The harness uses this to
	 *  prove the auto-cadence leg after parking Period far out for the force-driven legs. Authority. */
	void RescheduleCadence();

	/** Reflection-routed IsPhaseActive (THE LYRA PHASE WALL: no subsystem member symbol links from
	 *  outside LyraGame, so even this public UFUNCTION goes through ProcessEvent). Exposed static so
	 *  the harness (a different TU) shares the one tested path. */
	static bool IsPhaseActiveReflected(const UWorld* World, const FGameplayTag& PhaseTag);

	/** The match shell + the recurring window phase classes -- BP children of ULyraGamePhaseAbility
	 *  (the C++ subclass boundary above). Default-resolved by soft path in the ctor; a BP child of
	 *  this component could override. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	TSubclassOf<ULyraGamePhaseAbility> PlayingPhaseClass;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	TSubclassOf<ULyraGamePhaseAbility> WindowPhaseClass;

private:
	void ScheduleNextWindow();
	void OpenWindow();
	void CloseWindowNow();              // force-end the active window phase (cancel by class)
	bool IsWindowActive() const;

	/** Dual-broadcast an extraction-window announce: GameState multicast for clients + a local
	 *  server-world broadcast for the listen-server host (the NM_Client guard skips the host). */
	void BroadcastWindowAnnounce(const FGameplayTag& EventTag) const;

	FTimerHandle WindowOpenTimer;       // cadence (next opening)
	FTimerHandle WindowDurationTimer;   // this window's lifetime
	bool bWindowOpen = false;
};

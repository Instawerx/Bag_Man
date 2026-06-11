// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"
#include "GameplayTagContainer.h"

#include "AFLMatchPhaseComponent.generated.h"

class ULyraGamePhaseAbility;
class ULyraGamePhaseSubsystem;
class APlayerState;

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
 * THE FULL MATCH SPINE (S9 cycle 1): BeginPlay -> Warmup (30s, grants State.Match.Warmup to all
 * pawns -> fire/movement frozen; the zone stays Inactive for free since windows only open under
 * Playing) -> chains to Playing (auto-cancels Warmup, removes the warmup tag, snapshots each
 * player's Watts, arms the window cadence + the ActiveDuration timer) -> windows open/close on
 * cadence -> ActiveDuration elapses -> PostGame (auto-cancels Playing + .ExtractionWindow -> the
 * zone observer sweeps handles + any channeler self-cancels, NO explicit window force-close needed;
 * clears the cadence so no window reopens; grants State.Match.Ended -> fire/movement frozen again;
 * dual-broadcasts Event.Match.Ended PER PLAYER with this-match Watts) -> HOLDS (terminal, no restart
 * this cycle). Each StartPhase is reflection-routed (the phase wall). FULL SCOREBOARD (kills /
 * energy-extracted) is named S-later debt -- needs a per-player stats component; cycle 1 ships
 * only the wallet's Watts delta.
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

	/** Restart the WHOLE match spine from Warmup NOW, reading the duration cvars fresh (the driver
	 *  starts at BeginPlay on whatever cvars were set then; this lets the harness set COMPRESSED
	 *  durations and re-run deterministically). Clears all timers + match-tags + match-end flag,
	 *  cancels any live phase, then StartPhase(Warmup) again. Authority. */
	void RestartMatch();

	/** Reflection-routed IsPhaseActive (THE LYRA PHASE WALL: no subsystem member symbol links from
	 *  outside LyraGame, so even this public UFUNCTION goes through ProcessEvent). Exposed static so
	 *  the harness (a different TU) shares the one tested path. */
	static bool IsPhaseActiveReflected(const UWorld* World, const FGameplayTag& PhaseTag);

	/** The phase shells -- BP children of ULyraGamePhaseAbility (the C++ subclass boundary above).
	 *  Default-resolved by soft path in the ctor; a BP child of this component could override. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Match")
	TSubclassOf<ULyraGamePhaseAbility> WarmupPhaseClass;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Match")
	TSubclassOf<ULyraGamePhaseAbility> PlayingPhaseClass;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	TSubclassOf<ULyraGamePhaseAbility> WindowPhaseClass;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Match")
	TSubclassOf<ULyraGamePhaseAbility> PostGamePhaseClass;

private:
	// -- the match spine --
	void StartSpineFromWarmup();        // shared by BeginPlay + RestartMatch (reads cvars fresh)
	void EnterPlaying();                // WarmupTimer fire: chain Warmup -> Playing
	void EnterPostGame();               // ActiveTimer fire: Playing -> PostGame (terminal)
	void StartPhaseByClass(TSubclassOf<ULyraGamePhaseAbility> PhaseClass, const FGameplayTag& PhaseTag);
	void GrantMatchTagToAllPawns(const FGameplayTag& Tag);
	void RemoveMatchTagFromAllPawns(const FGameplayTag& Tag);
	void SnapshotMatchStartWatts();
	void BroadcastMatchEnded();         // per-player dual-broadcast with this-match Watts

	// -- the window cadence (extraction cycle 1) --
	void ScheduleNextWindow();
	void OpenWindow();
	void CloseWindowNow();              // force-end the active window phase (cancel by class)
	bool IsWindowActive() const;

	/** Dual-broadcast an announce: GameState multicast for clients + a local server-world broadcast
	 *  for the listen-server host (the NM_Client guard skips the host). Optional per-player Target +
	 *  Magnitude payload (the match-end Watts). */
	void BroadcastAnnounce(const FGameplayTag& EventTag, UObject* Target = nullptr, double Magnitude = 0.0) const;

	FTimerHandle WarmupTimer;           // warmup -> playing
	FTimerHandle ActiveTimer;           // playing -> postgame
	FTimerHandle WindowOpenTimer;       // cadence (next opening)
	FTimerHandle WindowDurationTimer;   // this window's lifetime
	bool bWindowOpen = false;
	bool bMatchEnded = false;           // PostGame reached -> cadence no-ops, terminal

	/** Per-player Watts at Playing start, keyed by PlayerState. The match-end payload per player =
	 *  GetWatts() - this snapshot (the wallet is per-player; each client shows its own). */
	TMap<TWeakObjectPtr<APlayerState>, int32> MatchStartWatts;
};

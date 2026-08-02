// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Match/AFLMatchPopulationComponent.h"

#include "AFLMatchPhaseComponent.generated.h"

class ULyraGamePhaseAbility;
class ULyraGamePhaseSubsystem;
class ULyraExperienceDefinition;
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
 * WHAT "FROZEN" MEANS HERE -- it blocks ABILITIES, not locomotion. State.Match.Warmup /
 * State.Match.Ended are loose tags that the fire and movement abilities carry in their constructors'
 * ActivationBlockedTags (AFLAG_Hitscan_Base.cpp:64, AFLGameplayAbility_Dash.cpp:37, and the rest of
 * the fire/movement set plus 13 BP weapon abilities). NOTHING touches the CharacterMovementComponent:
 * a "frozen" player still walks, strafes and jumps -- they cannot fire, dash, climb, sprint, slide,
 * roll, vault, wall-run or grab. The old wording ("fire/movement frozen") overstated it, which is
 * precisely how a real gap stayed hidden for five sessions.
 *
 * THE FULL MATCH SPINE (S9 cycle 1): BeginPlay -> Warmup (30s, grants State.Match.Warmup to all
 * pawns -> fire+movement ABILITIES blocked; the zone stays Inactive for free since windows only open
 * under Playing) -> chains to Playing (auto-cancels Warmup, removes the warmup tag, snapshots each
 * player's Watts, arms the window cadence + the ActiveDuration timer) -> windows open/close on
 * cadence -> ActiveDuration elapses -> PostGame (auto-cancels Playing + .ExtractionWindow -> the
 * zone observer sweeps handles + any channeler self-cancels, NO explicit window force-close needed;
 * clears the cadence so no window reopens; grants State.Match.Ended -> abilities blocked again;
 * dual-broadcasts Event.Match.Ended PER PLAYER with this-match Watts) -> HOLDS (terminal, no restart
 * this cycle). Each StartPhase is reflection-routed (the phase wall). FULL SCOREBOARD (kills /
 * energy-extracted) is named S-later debt -- needs a per-player stats component; cycle 1 ships
 * only the wallet's Watts delta.
 *
 * JOIN COVERAGE: every population write above is a one-shot sweep over who exists at the phase edge.
 * UAFLMatchPopulationComponent supplies the other half -- see ApplyJoinStateToPlayer below.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLMatchPhaseComponent : public UAFLMatchPopulationComponent
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

	/** Seconds left on the Warmup->Playing chain timer, or -1 when warmup is not running. This component
	 *  is a pure SERVER driver and replicates nothing, so it exposes the clock rather than publishing it:
	 *  UAFLRoundManagerComponent (which already ticks and already replicates) mirrors this into its
	 *  WarmupTimeRemaining for the HUD. Reads the timer manager directly -- no second countdown to drift. */
	float GetWarmupSecondsRemaining() const;

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

	/** Conclude the match NOW -- the proven PostGame conclusion (PostGame phase + State.Match.Ended freeze
	 *  + per-player Watts MATCH-COMPLETE broadcast). Idempotent via bMatchEnded. Called by the 480s time
	 *  path (EnterPostGame, when this component IS the authority) AND by an external match-end authority
	 *  (UAFLRoundManagerComponent at its win condition -- the round path). */
	void ConcludeMatch();

	/** Round-based mode hands match-end authority to the round FSM (sole authority, like it owns respawn):
	 *  the 480s time-based conclusion then no-ops (a clock ending a best-of mid-series is illogical). The
	 *  extraction-window cadence is UNAFFECTED. Default false = this component is the authority (time path). */
	void SetExternalMatchEndAuthority(bool bExternal) { bExternalMatchEndAuthority = bExternal; }

protected:
	/** JOIN COVERAGE (stage 1). Applies Warmup / Ended / NoDismember to a joiner's PlayerState ASC.
	 *  Warmup + Ended come from LIVE phase queries -- IsPhaseActiveReflected -- so there is no cache to
	 *  go stale. NoDismember is the one site with no live query, hence bCleanHealthMode. */
	virtual void ApplyJoinStateToPlayer(AController* NewPlayer, UAbilitySystemComponent* PlayerStateASC) override;

private:
	// -- the match spine --
	void StartSpineFromWarmup();        // shared by BeginPlay + RestartMatch (reads cvars fresh)
	void EnterPlaying();                // WarmupTimer fire: chain Warmup -> Playing
	void EnterPostGame();               // ActiveTimer fire: Playing -> PostGame (terminal)
	void StartPhaseByClass(TSubclassOf<ULyraGamePhaseAbility> PhaseClass, const FGameplayTag& PhaseTag);
	/** Sweep every pawn's ASC and SET the tag on/off. Returns the ASC count covered -- log it: a count
	 *  of 1 on a 6-player match is exactly what would have exposed the join gap on day one.
	 *  SET, not Add/Remove: the join handler writes the same tag to the same PlayerState ASC, and
	 *  refcounted adds would leave a stuck tag after one removal. Keeps the PAWN iteration on purpose --
	 *  that is what reaches the level-placed target dummy, which no join hook can ever see. */
	int32 SetMatchTagOnAllPawns(const FGameplayTag& Tag, bool bPresent);
	/** Deferred clean-health applier: registered on the experience-loaded delegate (reading the experience
	 *  any earlier asserts on LoadState). When the active experience OMITS the AFLDismember game feature
	 *  (Pro Mod / Melee), stamps State.Mode.NoDismember on every combatant ASC -- PlayerState ASCs (persist
	 *  across respawns) + pawns (the target dummy). Haywire keeps AFLDismember -> no-op. Also CACHES the
	 *  decision into bCleanHealthMode for the join path. */
	void OnExperienceLoaded_ApplyModeTags(const ULyraExperienceDefinition* Experience);
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
	bool bExternalMatchEndAuthority = false;   // round FSM owns match-end -> the 480s time-conclude no-ops

	/** SITE #1's cached decision -- the ONLY new state this fix adds, because it is the only one of the
	 *  five with no live query to ask. Set once in OnExperienceLoaded_ApplyModeTags. The join handler
	 *  reads THIS and never re-reads the experience: that re-read is what asserted on LoadState and
	 *  crashed the editor (see the deferral comment in BeginPlay). */
	bool bCleanHealthMode = false;

	/** Per-player Watts at Playing start, keyed by PlayerState. The match-end payload per player =
	 *  GetWatts() - this snapshot (the wallet is per-player; each client shows its own). */
	TMap<TWeakObjectPtr<APlayerState>, int32> MatchStartWatts;
};

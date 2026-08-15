// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Match/AFLMatchPopulationComponent.h"          // join coverage: sites #3 (pawn) + #4 (PlayerState ASC)
#include "Match/AFLEscrowLedger.h"                     // FAFLEscrowLedger -- held by value-in-shared-ptr, needs the full type
#include "Misc/Guid.h"   // A1.3b: FGuid MatchId + EGuidFormats
#include "GameFramework/GameplayMessageSubsystem.h"   // FGameplayMessageListenerHandle (member)
#include "AFLRoundRestartPolicy.h"                     // IAFLRoundRestartPolicy (the always-loaded AFLGameCore seam)
#include "AFLMatchCancelPolicy.h"                      // IAFLMatchCancelPolicy -- the abandonment seam (watch lives on the phase component)
#include "AFLMatchTierSource.h"                        // IAFLMatchTierSource (same seam -- bot aim tiering)

#include "AFLRoundManagerComponent.generated.h"

class APawn;
class APlayerState;
class ULyraHealthComponent;
struct FLyraVerbMessage;

/** The round-FSM phase (replicated to the HUD via OnRep_Phase). */
UENUM(BlueprintType)
enum class EAFLRoundPhase : uint8
{
	WarmUp,
	RoundActive,
	RoundEnd,
	HalfTime,
	MatchEnd
};

/** Why the round resolved (carried to clients alongside the winner via OnRep). */
UENUM(BlueprintType)
enum class EAFLRoundWinReason : uint8
{
	Elimination,
	Extraction,
	Timeout,
	Replay
};

/**
 * Why a match ended with NO RESULT. Both map to the backend's 'cancelled-refund' terminal state; they are kept
 * apart because they are different operational events -- one is players leaving, the other is players present
 * and the mode failing to resolve -- and a single "cancelled" line in the log could not tell you which.
 */
UENUM(BlueprintType)
enum class EAFLMatchCancelReason : uint8
{
	/** No human participants remain, and none returned within the grace window. */
	Abandoned,
	/** The round FSM produced MaxConsecutiveReplays no-score rounds in a row and cannot reach RoundsToWin. */
	ReplayCap,
	/**
	 * Nobody ever arrived. DISTINCT FROM Abandoned ON PURPOSE -- they are different events and a ledger that
	 * calls them the same thing misleads exactly when someone is reading it to explain a missing payout.
	 * Abandoned means the players were here and left; NoShow means the match was placed and never populated.
	 */
	NoShow
};

/**
 * UAFLRoundManagerComponent  (Arena_01 round-based extraction wrapper -- the code half)
 *
 * Server-authoritative round FSM for the Arena PvP win condition (Arena_01_DESIGN.md s1.1/s7/s12,
 * IRONICS_MAP_MODE_SPEC.md s1.1): 2 teams (ids 0/1), match = first to RoundsToWin (default 7, max 13),
 * a round is won by WIPING the enemy team OR completing a central-extract BANK; round timeout (100s)
 * resolves on higher banked progress -> core-holder -> no-score replay; side swap after HalfTimeAfterRound.
 *
 * PROVEN-SIBLING basis: mirrors UAFLMatchPhaseComponent (a UGameStateComponent in this module, arriving
 * via the experience AddComponents row; GetGameStateChecked<AGameStateBase>()->HasAuthority() gate;
 * timer-driven server FSM). DIVERGENCES (justified): (1) this component REPLICATES its state for the HUD
 * (the match-phase driver is server-only) -> SetIsReplicatedByDefault(true) + GetLifetimeReplicatedProps
 * + OnRep_*; (2) it TICKS (throttled) server-side to publish RoundTimeRemaining (the sibling never ticks).
 *
 * NET SAFETY: state is PLAIN replicated UPROPERTYs only -- NO custom net-serialized struct is introduced
 * (the AFLNetTypes rule governs NetSerialize/NetDeltaSerialize GAS structs, not GameState components).
 *
 * ⚠ THE MATCH IS GUARANTEED TO TERMINATE, and first-to-RoundsToWin is NOT that guarantee. A round can resolve
 * with NO winner (EAFLRoundWinReason::Replay -- nobody banked, nobody held the core), which scores nothing, so
 * a series of them never approaches RoundsToWin. Two bounds close that, and both end in the SAME place --
 * Server_CancelMatch, terminal state 'cancelled-refund':
 *
 *   MaxConsecutiveReplays    -- the mode cannot resolve. Bounds a stalled series (observed: 219 rounds at 0-0).
 *   the humanless watch      -- there is nobody left to resolve it. Bounds an empty one (observed: 80 minutes).
 *                               NOW ON UAFLMatchPhaseComponent, reaching this component through
 *                               IAFLMatchCancelPolicy::ServerCancelAbandoned. The guarantee is unchanged; only
 *                               the clock moved, so that battle royale inherits it too.
 *
 * Both were unbounded before, and the cost was not academic: a staked match that never ends holds its players'
 * escrow and its GameLift session for as long as the process lives.
 *
 * RECONCILED EXTERNAL SIGNALS (all server-side, named to recon):
 *  - Death/wipe  : ULyraHealthComponent::OnDeathStarted (FLyraHealth_DeathEvent; fires for AFL pawns --
 *                  UAFLDeathComponent drives LyraHealthComponent->StartDeath()). Bound per-pawn at round start.
 *  - Extract bank: the EXISTING GameplayMessage Event.Extraction.Complete (FLyraVerbMessage, Instigator=pawn),
 *                  broadcast on the SERVER world by UAFLAG_Extract after EarnWattsAuthority. We listen on the
 *                  server and resolve+replicate -- ZERO edits to the carry/extraction/banking code.
 *  - Team        : ULyraTeamSubsystem::FindTeamFromObject.
 *  - Respawn gate: AAFLGameMode::ControllerCanRestart consults ShouldBlockRestart() (the spawning manager's
 *                  ControllerCanRestart is private/non-virtual -> the gate lives on the game mode).
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLRoundManagerComponent : public UAFLMatchPopulationComponent, public IAFLRoundRestartPolicy, public IAFLMatchTierSource, public IAFLMatchCancelPolicy
{
	GENERATED_BODY()

public:
	UAFLRoundManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// -- tuning (Arena_01_DESIGN.md s12; all telemetry-tunable) --
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") int32 RoundsToWin = 7;
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") int32 HalfTimeAfterRound = 6;
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") float RoundTimeLimit = 100.f;
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") bool  bAllowMidRoundRespawn = false;
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") float RoundResetCountdown = 5.f;

	/**
	 * THE REPLAY BOUND. How many consecutive no-score (Replay) rounds the series tolerates before the match is
	 * cancelled. 0 disables the bound.
	 *
	 * ⚠ WITHOUT THIS THE MATCH LENGTH IS UNBOUNDED, and that is not a theoretical reading -- an S12 acceptance
	 * run produced 219 rounds over 6.5 hours at 0-0. A Replay is a legitimate outcome (nobody banked, nobody
	 * held the core, no wipe) and scores nothing, so a series where every round replays never approaches
	 * RoundsToWin. First-to-N bounds a match only if rounds actually score.
	 *
	 * CONSECUTIVE, not cumulative, and the difference is what keeps this from ending real matches: an isolated
	 * replay between scoring rounds is ordinary, and cumulative counting would eventually cancel a long, close,
	 * perfectly healthy series. A run of N is the stall itself. It also still bounds the match -- every run
	 * shorter than N is separated by a round that scores, and scores are capped at 2*RoundsToWin-1.
	 *
	 * 3 is deliberately low. Reaching it takes 3 x RoundTimeLimit (300s at the default) of both teams failing
	 * to bank, hold the core, or kill each other; a stalemate that deep is not a match anyone is playing.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") int32 MaxConsecutiveReplays = 3;

	// AbandonmentGraceSeconds DELETED 2026-08-15. The window now lives on UAFLMatchPhaseComponent with the
	// watch that reads it; this copy was left behind inert by the relocation and nothing read it. Its
	// reasoning and the S12 measurement moved with it.

	/** s6 traversal-density sampler: server-side per-living-pawn position emit cadence (seconds), the
	 *  traversal heatmap's data source. ~1-1.5s = cheap + dense enough for a flow read. Telemetry-tunable. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") float TraverseSampleInterval = 1.25f;

	// -- replicated state (drives the HUD via OnRep) --
	/** A1.3b: per-MATCH id (the whole Arena series), authored ONCE server-side at ServerStartMatch via
	 *  FGuid::NewGuid(). The server-authoritative matchId the earn push (later cycle) sends to the backend.
	 *  Replicated so clients can read it (proof + future HUD/telemetry). Stable for the series -- NOT per-round. */
	UPROPERTY(ReplicatedUsing = OnRep_MatchId, BlueprintReadOnly, Category = "AFL|Round") FGuid MatchId;
	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "AFL|Round") EAFLRoundPhase Phase = EAFLRoundPhase::WarmUp;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|Round") int32 CurrentRound = 0;
	// Score SLOTS (not team ids): slot 0/1 == ParticipatingTeams[0]/[1]. Names kept to avoid replication churn.
	UPROPERTY(ReplicatedUsing = OnRep_Score, BlueprintReadOnly, Category = "AFL|Round") int32 Team0Score = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Score, BlueprintReadOnly, Category = "AFL|Round") int32 Team1Score = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|Round") float RoundTimeRemaining = 0.f;

	/** The WARMUP countdown, mirrored from UAFLMatchPhaseComponent::GetWarmupSecondsRemaining (that
	 *  component owns the clock but is server-only and replicates nothing; this one already ticks and
	 *  already replicates, so it is the cheap place to publish). 0 outside warmup. Whole-second throttled
	 *  server-side -- the header only re-texts on a second boundary, so per-frame float churn would be
	 *  pure bandwidth for no visible gain. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|Round") float WarmupTimeRemaining = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|Round") bool  bSidesSwapped = false;

	/** The two participating team ids, resolved from ULyraTeamSubsystem at ServerStartMatch (NO magic
	 *  numbers -- the ShooterCore two-team stack uses ids 1/2, not 0/1). Slot 0/1 maps to Team0Score/
	 *  Team1Score. Replicated so the client HUD maps its local team to the right slot via SlotForTeam().
	 *  INDEX_NONE until the match starts. (C-array UPROPERTY -> not BlueprintReadOnly; read via SlotForTeam.) */
	UPROPERTY(Replicated) int32 ParticipatingTeams[2];

	/** Last resolution, replicated so OnRep can fire OnRoundResolved on clients (winner + reason for a UI toast). */
	UPROPERTY(ReplicatedUsing = OnRep_RoundResolved, BlueprintReadOnly, Category = "AFL|Round") int32 LastWinningTeam = INDEX_NONE;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|Round") EAFLRoundWinReason LastWinReason = EAFLRoundWinReason::Replay;

	/** UI/telemetry bind. Fires on the server at resolve, and on clients via OnRep_RoundResolved. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FAFLRoundResolved, int32 /*WinningTeam*/, EAFLRoundWinReason /*Reason*/);
	FAFLRoundResolved OnRoundResolved;

	/** Start the match FSM (WarmUp -> round 1). Authority; idempotent. Trigger wiring (match-phase Playing
	 *  entry / afl.Round.Start cheat) is Task 2; the cheat below drives it for the PIE watch. */
	void ServerStartMatch();

	// -- respawn-gate query surface (AAFLGameMode::ControllerCanRestart consults these) --
	UFUNCTION(BlueprintPure, Category = "AFL|Round") bool IsRoundActive() const { return Phase == EAFLRoundPhase::RoundActive; }

	/** A1.3b: the per-match id as a hyphenated string (the earn contract's matchId field). Server-authored,
	 *  replicated; empty-guid string until ServerStartMatch has run. Wired to nothing this cycle. */
	UFUNCTION(BlueprintPure, Category = "AFL|Round") FString GetMatchId() const { return MatchId.ToString(EGuidFormats::DigitsWithHyphens); }
	//~IAFLMatchTierSource -- the always-loaded seam AAFLBotController reads for aim tiering. Exposes ONLY
	// symmetric match state (round number, score line); nothing about an individual player's performance,
	// because bot difficulty keyed off how well the human is doing is rubber-banding and, with staking and
	// MMR in the mode, an integrity problem. The delta is sign-free: a consumer can brake on a blowout but
	// cannot tell which side is ahead, so it can never build a comeback mechanic.
	virtual int32 GetCurrentRoundNumber() const override { return CurrentRound; }
	virtual int32 GetRoundsToWin() const override        { return RoundsToWin; }
	virtual int32 GetScoreDelta() const override         { return FMath::Abs(Team0Score - Team1Score); }
	//~End of IAFLMatchTierSource

	//~IAFLRoundRestartPolicy -- the seam AAFLGameMode (always-loaded AFLGameCore) queries; routes to the
	// existing logic unchanged.
	virtual bool ShouldBlockRestart() const override { return IsRoundActive() && !bAllowMidRoundRespawn; }
	bool AreSidesSwapped() const { return bSidesSwapped; }

	/** IAFLRoundRestartPolicy: the side (0/1) team T is currently on -- its score slot XOR the half-time swap
	 *  (bSidesSwapped). INDEX_NONE for a non-participating team. Team ids are 1/2, so route through SlotForTeam
	 *  (never hardcode team->slot). The core spawn selector reads this to pick the team's fixed mirror side. */
	virtual int32 GetTeamSideIndex(int32 TeamId) const override
	{
		const int32 Slot = SlotForTeam(TeamId);
		return (Slot == INDEX_NONE) ? INDEX_NONE : (Slot ^ (bSidesSwapped ? 1 : 0));
	}

	/** The score slot (0 or 1) for a team id, or INDEX_NONE if not a participating team. The client HUD
	 *  maps its local team -> Team0Score/Team1Score with this; the server FSM uses it for all team attribution. */
	UFUNCTION(BlueprintPure, Category = "AFL|Round")
	int32 SlotForTeam(int32 TeamId) const
	{
		if (TeamId != INDEX_NONE)
		{
			if (TeamId == ParticipatingTeams[0]) { return 0; }
			if (TeamId == ParticipatingTeams[1]) { return 1; }
		}
		return INDEX_NONE;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** JOIN COVERAGE stage 1 (site #4): re-apply State.Round.NoRespawn to a joiner from the cached
	 *  bRespawnSuppressed. No live query exists for "is respawn currently suppressed" -- the flag mirrors
	 *  the last SetRoundRespawnSuppressed call and is written BEFORE that sweep runs. */
	virtual void ApplyJoinStateToPlayer(AController* NewPlayer, UAbilitySystemComponent* PlayerStateASC) override;

	/** JOIN COVERAGE stage 2 (site #3): bind OnDeathStarted on a pawn as it is possessed -- on join AND
	 *  on every respawn. This is the site that made the round hang: a pawn spawning after round start was
	 *  never bound, so its death never reached HandlePlayerDeath and never counted toward AliveCount. */
	virtual void ApplyJoinStateToPawn(AController* Controller, APawn* NewPawn) override;

	// -- the server FSM --
	void Server_BeginRound();
	void Server_ResolveRound(int32 WinningTeamId, EAFLRoundWinReason Reason);
	void Server_OnRoundTimeout();
	void Server_BetweenRounds();                         // RoundEnd countdown fire: (halftime?) -> reset -> begin
	void Server_EnterHalfTime();                         // toggles bSidesSwapped
	void Server_EndMatch(int32 WinningTeamId);

	/**
	 * End the match with NO RESULT and refund the stake.
	 *
	 * The counterpart to Server_EndMatch, and the two are mutually exclusive by the bMatchConcluded latch:
	 * `/settle-match` CLAIMS a matchId with a conditional write and can never be re-settled, so a match that
	 * both concluded and cancelled would have its outcome decided by whichever HTTP request happened to arrive
	 * first. The latch makes that race unreachable in-process; the backend's claim is the backstop, not the
	 * plan. Both functions run on the game thread, so the check-and-set needs no synchronisation.
	 *
	 * Refunds BEFORE concluding -- the opposite order to Server_EndMatch, deliberately. That function concludes
	 * first so players are not made to wait on a backend round-trip to see the match end; in a cancellation the
	 * usual reason is that there are no players left to wait, and the refund is the entire purpose of the path.
	 * Both calls are fire-and-forget, so the ordering costs nothing either way.
	 */
	void Server_CancelMatch(EAFLMatchCancelReason Reason);

	void Server_ResetRoundActors();                      // force-respawn all (fresh pawns clear carry/extract for free)

	// -- reconciled external signals --
	UFUNCTION() void HandlePlayerDeath(AActor* OwningActor);                       // ULyraHealthComponent::OnDeathStarted
	void HandleExtractionBanked(FGameplayTag Channel, const FLyraVerbMessage& Message);  // Event.Extraction.Complete
	UFUNCTION() void HandlePlayingPhaseActive(const FGameplayTag& PhaseTag);       // Task 2: AFL.GamePhase.Playing start -> ServerStartMatch (ULyraGamePhaseSubsystem observer, mirrors AAFLExtractionZone)

	int32 ComputeTimeoutWinner() const;                  // higher banked -> core holder -> INDEX_NONE
	void EmitRoundTelemetry(int32 WinningTeamId, EAFLRoundWinReason Reason) const;

	UFUNCTION() void OnRep_Phase();
	UFUNCTION() void OnRep_Score();
	UFUNCTION() void OnRep_RoundResolved();
	UFUNCTION() void OnRep_MatchId();

private:
	bool HasAuth() const;                                // GetOwner()->HasAuthority() (the GameState actor)
	int32 AliveCount(int32 TeamId) const;                // enumerate PlayerArray by team, count !IsDeadOrDying

	/** Human PARTICIPANTS currently connected -- not bots, not pure spectators. */
	int32 CountHumanParticipants() const;

	//~IAFLMatchCancelPolicy -- the abandonment seam. The WATCH itself moved to UAFLMatchPhaseComponent on
	// 2026-08-15 (it was in the MATCH PLAY experiences and in neither BR one, so it could never see an
	// abandoned battle royale). This component still owns what abandonment MEANS here: stopping the round FSM,
	// unbinding deaths, and refunding off the ledger.
	virtual bool IsMatchLiveForAbandonment() const override;
	virtual void ServerCancelAbandoned() override;
	//~End of IAFLMatchCancelPolicy

	int32 TeamHoldingCore() const;                       // the team with a pawn carrying State.Extracting (else INDEX_NONE)
	void BindDeathDelegates();                           // full reconcile: rebind OnDeathStarted across PlayerArray
	/** Bind one pawn's health component, guarded. AddDynamic is NOT idempotent -- a double bind fires
	 *  HandlePlayerDeath twice per death, double-decrements the AliveCount check and ends rounds early.
	 *  Shared by the round-start reconcile and the per-possession join path, so it MUST stay guarded. */
	bool BindDeathDelegateForPawn(APawn* Pawn);
	void UnbindDeathDelegates();
	void SetPhaseAuthoritative(EAFLRoundPhase NewPhase);  // set + drive OnRep locally (listen-host)
	void SetRoundRespawnSuppressed(bool bSuppressed);     // apply/remove State.Round.NoRespawn on every player ASC -> round FSM is the lone respawn authority

	bool bMatchStarted = false;

	/**
	 * THE SINGLE-CONCLUSION LATCH. Set by Server_EndMatch and Server_CancelMatch, checked by both.
	 *
	 * A match has exactly one terminal report because settlement CLAIMS the matchId with a conditional write --
	 * a second report is not an overwrite, it is a race whose winner is decided by network timing. bMatchStarted
	 * cannot serve here: it marks the match as begun and stays true across the end, so it says nothing about
	 * whether an outcome has already been sent.
	 */
	bool bMatchConcluded = false;

	/** DEFECT 1's counter: no-score rounds since the last one that scored. Reset by any scoring resolution. */
	int32 ConsecutiveReplays = 0;

	/** DEFECT 2's accumulator: seconds since the last human participant was seen. 0 whenever one is present. */
	float HumanlessSeconds = 0.f;

	/**
	 * ARRIVAL GATE. Has ANY human been seen in this match yet? DERIVED at ServerStartMatch from
	 * CountHumanParticipants() > 0 -- never written by another component.
	 *
	 * ⚠ THIS IS THE ONE BIT THE ABANDONMENT WATCH WAS MISSING. CountHumanParticipants() == 0 has two
	 * completely different meanings -- "everyone left" and "nobody has arrived yet" -- and the watch could
	 * not tell them apart, so it spent its 60s grace window on ARRIVAL LATENCY and cancelled four matches
	 * that were about to be populated.
	 *
	 * IT IS NOW TRUE BY CONSTRUCTION. UAFLMatchPhaseComponent holds Warmup->Playing until a human is present,
	 * so by the time ServerStartMatch runs the ambiguity no longer exists and 0 can only mean they LEFT. The
	 * latch is kept rather than deleted because it is the ASSERTION that this holds: if it is ever false at
	 * match start, the phase gate was bypassed (a cheat-started match, a replay path) and the watch stays
	 * disarmed instead of cancelling a match nobody was ever placed into.
	 *
	 * "EXPECTED" IS IMPLICIT, AND THAT IS DELIBERATE -- no roster match is performed. Under GameLift a client
	 * cannot connect at all without a playerSessionId, /claim-session mints one only for a caller holding a
	 * ready row naming that game session, and AFLGameMode REFUSES a connection with no ?PlayerSessionId= at
	 * PreLogin. So the only humans who can appear ARE the placed ones. Matching PlayFabIds here would add a
	 * second roster copy that can drift out of sync with the one the connection layer already enforces.
	 */
	bool bAnyHumanEverJoined = false;

	/**
	 * WHO WAS DEBITED, captured at match start and held for the whole match.
	 *
	 * ⚠ THIS IS WHY ABANDONMENT CAN BE REFUNDED AT ALL. A settlement body needs a playFabId and an amount per
	 * entry; every other path in this codebase reads those off GS->PlayerArray. In the case this exists for the
	 * PlayerArray is EMPTY -- the match is ending precisely because everyone left. The snapshot is taken while
	 * the roster is still there and outlives it.
	 *
	 * Null for an unstaked match, an unwired economy, or a refused escrow -- in each of those nothing was taken,
	 * so a cancellation has nothing to give back.
	 */
	TSharedPtr<FAFLEscrowLedger> EscrowLedger;
	/** SITE #4's cached decision -- mirrors the last SetRoundRespawnSuppressed call, so a joiner can be
	 *  given the state that is true NOW without replaying the round history. Written before the sweep. */
	bool bRespawnSuppressed = false;
	int32 Team0Banked = 0;                               // per-round banked accumulator (timeout tiebreak)
	int32 Team1Banked = 0;
	float TraverseSampleAccum = 0.f;                     // s6 traversal sampler throttle accumulator (Tick)
	FTimerHandle RoundTimerHandle;                       // round timeout
	FTimerHandle ResetTimerHandle;                       // RoundEnd -> between-rounds -> begin
	FGameplayMessageListenerHandle ExtractListenerHandle;
	TArray<TWeakObjectPtr<ULyraHealthComponent>> BoundHealthComps;
	/** Lazily resolved sibling on the GameState -- the warmup clock's owner. Cached so the countdown
	 *  publish is not a FindComponentByClass every frame of warmup. */
	TWeakObjectPtr<class UAFLMatchPhaseComponent> PhaseComp;
};

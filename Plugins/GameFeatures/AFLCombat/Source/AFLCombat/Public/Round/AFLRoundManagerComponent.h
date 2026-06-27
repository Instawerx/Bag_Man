// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"   // FGameplayMessageListenerHandle (member)
#include "AFLRoundRestartPolicy.h"                     // IAFLRoundRestartPolicy (the always-loaded AFLGameCore seam)

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
class AFLCOMBAT_API UAFLRoundManagerComponent : public UGameStateComponent, public IAFLRoundRestartPolicy
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

	/** s6 traversal-density sampler: server-side per-living-pawn position emit cadence (seconds), the
	 *  traversal heatmap's data source. ~1-1.5s = cheap + dense enough for a flow read. Telemetry-tunable. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") float TraverseSampleInterval = 1.25f;

	// -- replicated state (drives the HUD via OnRep) --
	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "AFL|Round") EAFLRoundPhase Phase = EAFLRoundPhase::WarmUp;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|Round") int32 CurrentRound = 0;
	// Score SLOTS (not team ids): slot 0/1 == ParticipatingTeams[0]/[1]. Names kept to avoid replication churn.
	UPROPERTY(ReplicatedUsing = OnRep_Score, BlueprintReadOnly, Category = "AFL|Round") int32 Team0Score = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Score, BlueprintReadOnly, Category = "AFL|Round") int32 Team1Score = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|Round") float RoundTimeRemaining = 0.f;
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
	//~IAFLRoundRestartPolicy -- the seam AAFLGameMode (always-loaded AFLGameCore) queries; routes to the
	// existing logic unchanged.
	virtual bool ShouldBlockRestart() const override { return IsRoundActive() && !bAllowMidRoundRespawn; }
	bool AreSidesSwapped() const { return bSidesSwapped; }

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

	// -- the server FSM --
	void Server_BeginRound();
	void Server_ResolveRound(int32 WinningTeamId, EAFLRoundWinReason Reason);
	void Server_OnRoundTimeout();
	void Server_BetweenRounds();                         // RoundEnd countdown fire: (halftime?) -> reset -> begin
	void Server_EnterHalfTime();                         // toggles bSidesSwapped
	void Server_EndMatch(int32 WinningTeamId);
	void Server_ResetRoundActors();                      // force-respawn all (fresh pawns clear carry/extract for free)

	// -- reconciled external signals --
	UFUNCTION() void HandlePlayerDeath(AActor* OwningActor);                       // ULyraHealthComponent::OnDeathStarted
	void HandleExtractionBanked(FGameplayTag Channel, const FLyraVerbMessage& Message);  // Event.Extraction.Complete

	int32 ComputeTimeoutWinner() const;                  // higher banked -> core holder -> INDEX_NONE
	void EmitRoundTelemetry(int32 WinningTeamId, EAFLRoundWinReason Reason) const;

	UFUNCTION() void OnRep_Phase();
	UFUNCTION() void OnRep_Score();
	UFUNCTION() void OnRep_RoundResolved();

private:
	bool HasAuth() const;                                // GetOwner()->HasAuthority() (the GameState actor)
	int32 AliveCount(int32 TeamId) const;                // enumerate PlayerArray by team, count !IsDeadOrDying
	int32 TeamHoldingCore() const;                       // the team with a pawn carrying State.Extracting (else INDEX_NONE)
	void BindDeathDelegates();                           // bind OnDeathStarted on every current pawn
	void UnbindDeathDelegates();
	void SetPhaseAuthoritative(EAFLRoundPhase NewPhase);  // set + drive OnRep locally (listen-host)

	bool bMatchStarted = false;
	int32 Team0Banked = 0;                               // per-round banked accumulator (timeout tiebreak)
	int32 Team1Banked = 0;
	float TraverseSampleAccum = 0.f;                     // s6 traversal sampler throttle accumulator (Tick)
	FTimerHandle RoundTimerHandle;                       // round timeout
	FTimerHandle ResetTimerHandle;                       // RoundEnd -> between-rounds -> begin
	FGameplayMessageListenerHandle ExtractListenerHandle;
	TArray<TWeakObjectPtr<ULyraHealthComponent>> BoundHealthComps;
};

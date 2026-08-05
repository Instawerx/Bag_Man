// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Match/AFLMatchPopulationComponent.h"          // join coverage: sites #3 (pawn death-bind) + #4 (PlayerState ASC)
#include "Misc/Guid.h"                                    // FGuid MatchId + EGuidFormats (staking contract id)
#include "AFLRoundRestartPolicy.h"                        // IAFLRoundRestartPolicy (the always-loaded AFLGameCore seam)

#include "AFLBattleRoyaleComponent.generated.h"

class APawn;
class APlayerState;
class ULyraHealthComponent;

/** The BR match phase (replicated to the HUD via OnRep_Phase). */
UENUM(BlueprintType)
enum class EAFLBRPhase : uint8
{
	WarmUp,
	Playing,
	MatchEnd
};

/**
 * UAFLBattleRoyaleComponent  (Battle Royale win condition -- P0.5 spike, S1)
 *
 * Server-authoritative LAST-STANDING FSM for N solo participants, with placement (1..N). It is the BR
 * sibling of UAFLRoundManagerComponent (Arena 2-team best-of) and UAFLDeathmatchRankComponent (Melee) --
 * a MATCH-STRUCTURE layer added via the experience AddComponents row, ORTHOGONAL to the Haywire/Pro Mod
 * combat split (so a BR experience exists per GE). Scope of the spike:
 *   - SOLO: every APlayerState in PlayerArray (human + bots) is its own participant.
 *   - LAST-STANDING: match ends when AliveParticipants <= SurvivorsToWin (1). The survivor = placement 1.
 *   - PLACEMENT: each elimination books the dier's finishing place (N, N-1, ...), for the staking payout /
 *     league rank feed (wired later; today emitted to telemetry + the resolved delegate).
 *   - NO-RESPAWN baseline: dead is out for the match (State.Round.NoRespawn on the PlayerState ASC +
 *     IAFLRoundRestartPolicy::ShouldBlockRestart -> AAFLGameMode::ControllerCanRestart). The ShantyTown
 *     water "respawn-on-land" rule is a MAP-side exception layered later, not here.
 *
 * REUSE (proven-sibling basis, UAFLRoundManagerComponent):
 *   - Death signal   : ULyraHealthComponent::OnDeathStarted, bound per-pawn (round-start reconcile + the
 *                      per-possession join hook from UAFLMatchPopulationComponent -- covers mid-match spawns).
 *   - Match start    : observe AFL.GamePhase.Playing on ULyraGamePhaseSubsystem (the reflective phase-wall
 *                      bind AAFLExtractionZone/round manager use) -> ServerStartMatch. Also afl.BR.Start cheat.
 *   - Match id       : FGuid authored once at ServerStartMatch, replicated -- the staking contract id.
 *   - Match end      : hands the resident UAFLMatchPhaseComponent external match-end authority, then concludes
 *                      via its proven PostGame machinery (freeze + Watts banner) at last-standing.
 *
 * NET SAFETY: plain replicated UPROPERTYs only (no custom net-serialized struct), per the round manager rule.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLBattleRoyaleComponent : public UAFLMatchPopulationComponent, public IAFLRoundRestartPolicy
{
	GENERATED_BODY()

public:
	UAFLBattleRoyaleComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Survivors remaining that ends the match. 1 = classic last-standing (solo). Telemetry-tunable. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|BR") int32 SurvivorsToWin = 1;

	// -- replicated state (drives the HUD via OnRep) --
	/** Per-MATCH id, authored ONCE server-side at ServerStartMatch. The staking/earn contract's matchId. */
	UPROPERTY(ReplicatedUsing = OnRep_MatchId, BlueprintReadOnly, Category = "AFL|BR") FGuid MatchId;
	UPROPERTY(ReplicatedUsing = OnRep_Phase,   BlueprintReadOnly, Category = "AFL|BR") EAFLBRPhase Phase = EAFLBRPhase::WarmUp;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|BR") int32 AlivePlayers = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AFL|BR") int32 TotalParticipants = 0;
	/** Winner's PlayerId (APlayerState::GetPlayerId), or INDEX_NONE for none/draw. Replicated for a UI toast. */
	UPROPERTY(ReplicatedUsing = OnRep_Resolved, BlueprintReadOnly, Category = "AFL|BR") int32 WinnerPlayerId = INDEX_NONE;

	/** Fires on the server at resolve, and on clients via OnRep_Resolved (winner may be null on a draw). */
	DECLARE_MULTICAST_DELEGATE_OneParam(FAFLBRResolved, APlayerState* /*Winner*/);
	FAFLBRResolved OnBattleRoyaleResolved;

	/** Start the BR FSM (WarmUp -> Playing). Authority; idempotent (bMatchStarted guard). */
	void ServerStartMatch();

	UFUNCTION(BlueprintPure, Category = "AFL|BR") bool IsMatchActive() const { return Phase == EAFLBRPhase::Playing; }
	UFUNCTION(BlueprintPure, Category = "AFL|BR") FString GetMatchId() const { return MatchId.ToString(EGuidFormats::DigitsWithHyphens); }

	/** Finishing place (1..N) booked for a player, or 0 if not yet resolved. */
	UFUNCTION(BlueprintPure, Category = "AFL|BR") int32 GetPlacementForPlayer(const APlayerState* PS) const;

	//~IAFLRoundRestartPolicy -- BR is no-respawn for the whole match: block every restart once Playing begins.
	// GetTeamSideIndex default (INDEX_NONE) is correct -- BR has no fixed team sides.
	virtual bool ShouldBlockRestart() const override { return bRespawnBlocked; }
	//~End IAFLRoundRestartPolicy

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** JOIN COVERAGE #4: re-apply the no-respawn tag to a joiner from the cached bRespawnBlocked. */
	virtual void ApplyJoinStateToPlayer(AController* NewPlayer, UAbilitySystemComponent* PlayerStateASC) override;
	/** JOIN COVERAGE #3: bind OnDeathStarted on a pawn as it is possessed (join AND respawn). */
	virtual void ApplyJoinStateToPawn(AController* Controller, APawn* NewPawn) override;

	UFUNCTION() void HandlePlayingPhaseActive(const FGameplayTag& PhaseTag);   // AFL.GamePhase.Playing -> ServerStartMatch
	UFUNCTION() void HandlePlayerDeath(AActor* OwningActor);                    // ULyraHealthComponent::OnDeathStarted

	UFUNCTION() void OnRep_Phase();
	UFUNCTION() void OnRep_MatchId();
	UFUNCTION() void OnRep_Resolved();

private:
	bool HasAuth() const;
	/** Count living solo participants (PlayerArray entries with a non-dead pawn). Optionally returns the LAST
	 *  living PlayerState found -- used to identify the sole survivor at last-standing. */
	int32 AliveParticipants(APlayerState** OutLastAlive = nullptr) const;
	void SetPhaseAuthoritative(EAFLBRPhase NewPhase);
	void SetRespawnBlocked(bool bBlocked);                 // apply/remove State.Round.NoRespawn on every player ASC
	void Server_EndMatch(APlayerState* Winner);

	void BindDeathDelegates();                             // full reconcile at match start
	bool BindDeathDelegateForPawn(APawn* Pawn);            // guarded (AddDynamic is not idempotent)
	void UnbindDeathDelegates();

	bool bMatchStarted = false;
	bool bRespawnBlocked = false;                          // cached source-of-truth for the join site + ShouldBlockRestart
	int32 NextPlacement = 0;                               // = the finishing place the next elimination books (N, N-1, ...)

	/** Booked finishing places, keyed by PlayerState (survives pawn death; the ASC/PS is the stable identity). */
	TMap<TWeakObjectPtr<APlayerState>, int32> Placements;
	TArray<TWeakObjectPtr<ULyraHealthComponent>> BoundHealthComps;
};

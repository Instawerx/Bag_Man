// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Match/AFLMatchPopulationComponent.h"          // join coverage: sites #3 (pawn death-bind) + #4 (PlayerState ASC)
#include "Misc/Guid.h"                                    // FGuid MatchId + EGuidFormats (staking contract id)
#include "Match/AFLMatchResultTypes.h"                     // FAFLMatchParticipant -- DepartedParticipants holds them BY VALUE
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

	/** BLOCK 177 instrumentation (dev-only, `#if !UE_BUILD_SHIPPING`). Logs the component's FULL belief state:
	 *  the `AFL_BR_STATE` summary (eliminated / alive / survivorsToWin / endConditionMet) + a per-participant
	 *  roster line each (name | alive | teamId | placement). Team id is resolved from `ULyraTeamSubsystem`
	 *  (the same source the round manager reads). Called on every elimination, once at match start, and by
	 *  the `afl.BR.DumpState` cheat -- so a stall is a READ, not an inference. Pure logging; changes nothing. */
	void LogBeliefState(const FString& Context, const APlayerState* JustEliminated = nullptr) const;

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

	/**
	 * FORFEIT. A human who disconnects mid-match takes their placement AT THE MOMENT OF LEAVING -- operator
	 * ruling 2026-08-15, and it is literal: the rung is booked here, in the logout broadcast, not when a grace
	 * expires. Same mechanism the ladder already runs on death, different trigger.
	 *
	 * WHY BOOK NOW RATHER THAN AT A GRACE. It keeps the ladder dense with no ghost sitting in the field and
	 * nothing waiting on a player who may never return. AAFLGameMode::ReconnectGraceSeconds is unaffected and
	 * keeps doing its own job -- holding the GameLift seat -- which is honest, because BR reconnect does not
	 * work today regardless: respawn is blocked for the match, so a returning player arrives bodiless. If BR
	 * reconnect is ever built, this ruling reopens with it.
	 *
	 * ⚠ REFUNDING A LEAVER IS EXPLOITABLE -- whoever is behind simply leaves. The stake stays escrowed and the
	 * match settles normally. The ONLY refund case is EVERY human leaving, which is the abandonment watch and
	 * not this. The opposite ruling from 2026-08-09 is still written in AFLGameMode.h and is now superseded.
	 */
	void HandlePlayerLoggedOut(AGameModeBase* GameMode, AController* Exiting);

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

	/**
	 * Refund the pot and end the match with NO RESULT. Mirrors UAFLRoundManagerComponent::Server_CancelMatch.
	 *
	 * ⚠ NOT ReportMatchEnd. A cancelled match has no winner, and posting a settlement would both pay a curve
	 * against an outcome nobody played and move a rating -- a ladder you can farm by disconnecting. The refund
	 * is built from the LEDGER, which is the only thing that still describes the pot once the players have gone.
	 *
	 * ⚠ ITS TRIGGER IS NOT WIRED YET, AND THAT IS STATED RATHER THAN HIDDEN. Battle royale has no abandonment
	 * watch: UAFLRoundManagerComponent owns that for MATCH PLAY and is in neither BR experience. So today the
	 * only caller is EndPlay on an unsettled staked match. A BR-side humanless watch is its own task.
	 */
	void Server_CancelMatch(const FString& ReasonText);

	void BindDeathDelegates();                             // full reconcile at match start
	bool BindDeathDelegateForPawn(APawn* Pawn);            // guarded (AddDynamic is not idempotent)
	void UnbindDeathDelegates();

	bool bMatchStarted = false;
	bool bRespawnBlocked = false;                          // cached source-of-truth for the join site + ShouldBlockRestart
	bool bLogoutHookBound = false;                         // OnGameModeLogoutEvent is global -- bind once, not per restart
	int32 NextPlacement = 0;                               // = the finishing place the next elimination books (N, N-1, ...)

	/** Booked finishing places, keyed by PlayerState (survives pawn death; the ASC/PS is the stable identity). */
	TMap<TWeakObjectPtr<APlayerState>, int32> Placements;

	/** Book `PS` at the next free rung. False when they already hold one -- OnDeathStarted can fire twice, and
	 *  a forfeit can follow a death in the same frame. Shared by the death and forfeit triggers so the two
	 *  cannot book differently. */
	bool BookPlacement(APlayerState* PS);

	/**
	 * Participants who LEFT, captured whole at the moment they left.
	 *
	 * ⚠ A PlayerState POINTER WOULD BE USELESS HERE. A forfeiter is destroyed with their controller, so by
	 * match end `Placements` holds a stale weak pointer and PlayerArray cannot describe them at all. The
	 * reconcile id, the bot flag and the rung are read once, while they still exist, and carried to the result
	 * builder. Without this a forfeiter silently vanishes from the result -- taking their rung with them and
	 * leaving the ladder non-dense, which is the exact defect that stranded a pot.
	 */
	TArray<FAFLMatchParticipant> DepartedParticipants;

	/**
	 * What this server took at match start, and from whom. NULL for an unstaked match -- LEAGUE PLAY has no
	 * buy-in, so there is no pot and nothing a cancellation would refund.
	 *
	 * ⚠ HELD FOR THE MATCH LIFETIME, and that is the entire reason the type exists. At abandonment the players
	 * are gone: PlayerArray is empty and the PlayerStates are destroyed, so a refund built from live state
	 * refunds nobody. This snapshot outlives the players it describes.
	 *
	 * Latched false once settled or refunded, so the EndPlay backstop cannot post a second settlement against
	 * a pot that has already moved.
	 */
	TSharedPtr<struct FAFLEscrowLedger> EscrowLedger;
	bool bEconomySettled = false;
	TArray<TWeakObjectPtr<ULyraHealthComponent>> BoundHealthComps;
};

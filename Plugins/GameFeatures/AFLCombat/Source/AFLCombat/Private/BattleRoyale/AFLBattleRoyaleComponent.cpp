// Copyright C12 AI Gaming. All Rights Reserved.

#include "BattleRoyale/AFLBattleRoyaleComponent.h"

#include "AFLCombat.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"   // observe AFL.GamePhase.Playing -> ServerStartMatch (the proven phase-observer path)
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Phases/AFLMatchPhaseComponent.h"
#include "GameFramework/GameModeBase.h"   // FGameModeEvents -- the forfeit trigger
#include "Teams/AFLReconcileIdComponent.h" // the forfeiter identity, captured before their state is destroyed
#include "TimerManager.h"                  // next-tick end-condition re-check after a forfeit
#include "Match/AFLMatchReporter.h"      // EscrowFreeForAll + BuildFieldResult + ReportMatchEnd/Cancelled
#include "Match/AFLEscrowLedger.h"       // the pot snapshot held for the match lifetime
#include "Match/AFLMatchResultTypes.h"   // FAFLMatchResult
#include "Teams/LyraTeamSubsystem.h"   // BLOCK 177: runtime team id for the belief-state roster (same source as the round manager)
#include "Telemetry/AFLCombatTelemetry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBattleRoyaleComponent)

// Same no-respawn tag the round manager uses: the cloned GA_AFL_AutoRespawn skips its respawn node while
// this is on the owning (PlayerState) ASC. UE dedups native+ini; AFLCombatTags.ini is the spec source.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Round_NoRespawn_BR, "State.Round.NoRespawn");

// The match-phase "Playing" tag (UAFLMatchPhaseComponent starts it at the Warmup->Playing edge). Its driver
// tag is file-local to that .cpp, so define our own static for the same string (UE dedups) and observe it
// WITHOUT linking the driver's symbol -- exactly how AAFLExtractionZone / the round manager do it.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Playing_BR, "AFL.GamePhase.Playing");

UAFLBattleRoyaleComponent::UAFLBattleRoyaleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Event-driven (deaths + phase start); no per-frame work in the spike -> never tick. The zone system
	// (S2) will own match time, not this component.
	PrimaryComponentTick.bCanEverTick = false;
	// Replicates its state to drive the HUD (sibling round manager does the same; the match-phase driver is server-only).
	SetIsReplicatedByDefault(true);
}

void UAFLBattleRoyaleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLBattleRoyaleComponent, MatchId);
	DOREPLIFETIME(UAFLBattleRoyaleComponent, Phase);
	DOREPLIFETIME(UAFLBattleRoyaleComponent, AlivePlayers);
	DOREPLIFETIME(UAFLBattleRoyaleComponent, TotalParticipants);
	DOREPLIFETIME(UAFLBattleRoyaleComponent, WinnerPlayerId);
}

bool UAFLBattleRoyaleComponent::HasAuth() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UAFLBattleRoyaleComponent::BeginPlay()
{
	Super::BeginPlay();

	// Clients only consume the replicated state via OnRep -- mirror the sibling authority gate.
	if (!GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		return;
	}

	SetPhaseAuthoritative(EAFLBRPhase::WarmUp);

	// Auto-start on the natural Warmup->Playing transition (no cheat needed). There is no delegate on
	// UAFLMatchPhaseComponent to bind (EnterPlaying is private + broadcasts nothing); the transition is
	// observable ONLY as the AFL.GamePhase.Playing phase-start on the Lyra GamePhaseSubsystem. Register the
	// SAME reflective observer AAFLExtractionZone/round manager use (THE LYRA PHASE WALL: the C++ WhenPhase*
	// overloads aren't LYRAGAME_API + the K2_ UFUNCTIONs are protected -> bind via ProcessEvent). ExactMatch
	// -> fires once on the Playing entry (not the .ExtractionWindow child). Already past the authority gate.
	if (ULyraGamePhaseSubsystem* PhaseSub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(GetWorld()))
	{
		struct FK2WhenPhaseParams
		{
			FGameplayTag PhaseTag;
			EPhaseTagMatchType MatchType = EPhaseTagMatchType::ExactMatch;
			FLyraGamePhaseTagDynamicDelegate WhenPhase;
		};
		if (UFunction* Fn = PhaseSub->FindFunction(TEXT("K2_WhenPhaseStartsOrIsActive")))
		{
			FK2WhenPhaseParams Params;
			Params.PhaseTag = TAG_AFL_GamePhase_Playing_BR;
			Params.MatchType = EPhaseTagMatchType::ExactMatch;
			Params.WhenPhase.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UAFLBattleRoyaleComponent, HandlePlayingPhaseActive));
			PhaseSub->ProcessEvent(Fn, &Params);
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR bind ok -- observing AFL.GamePhase.Playing -> ServerStartMatch."));
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_BR bind FAILED -- K2_WhenPhaseStartsOrIsActive not found (fallback: afl.BR.Start)."));
		}
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_BR bind SKIP -- no ULyraGamePhaseSubsystem in this world (fallback: afl.BR.Start)."));
	}
}

void UAFLBattleRoyaleComponent::HandlePlayingPhaseActive(const FGameplayTag& PhaseTag)
{
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR Playing phase active (tag=%s) -> ServerStartMatch."), *PhaseTag.ToString());
	ServerStartMatch();
}

void UAFLBattleRoyaleComponent::Server_CancelMatch(const FString& ReasonText)
{
	if (!HasAuth() || bEconomySettled)
	{
		return;   // the latch shared with Server_EndMatch -- one terminal per match, never a race
	}
	bEconomySettled = true;

	if (!EscrowLedger.IsValid() || !EscrowLedger->IsStaked())
	{
		// Nothing was taken, so there is nothing to give back. The ordinary case for every LEAGUE PLAY field.
		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_BR: match %s cancelled (%s) -- unstaked, no pot to refund."), *GetMatchId(), *ReasonText);
		return;
	}

	UE_LOG(LogAFLCombat, Warning,
		TEXT("AFL_BR: match %s CANCELLED (%s) -- refunding %d entr(ies), NO rake, NO rating."),
		*GetMatchId(), *ReasonText, EscrowLedger->Entries.Num());
	FAFLMatchReporter::ReportMatchCancelled(this, *EscrowLedger, ReasonText);
}

void UAFLBattleRoyaleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ⚠ THE BACKSTOP, NOT THE INTENDED PATH. A staked field whose pot never moved is money held against a
	// match that will never report, so tearing the component down without refunding would strand it. This
	// fires only when the latch is still open -- a settled or already-refunded match returns immediately.
	//
	// It is NOT an abandonment watch and must not be mistaken for one: EndPlay also runs on level travel and
	// server shutdown, so it recovers the pot rather than diagnosing why the match died. The real watch now
	// exists -- UAFLMatchPhaseComponent owns it and reaches this component through
	// IAFLMatchCancelPolicy::ServerCancelAbandoned -- so this is the backstop behind it rather than the only
	// line of defence it was when written.
	if (HasAuth() && !bEconomySettled && EscrowLedger.IsValid() && EscrowLedger->IsStaked())
	{
		Server_CancelMatch(TEXT("component torn down with an unsettled pot"));
	}

	UnbindDeathDelegates();
	Super::EndPlay(EndPlayReason);
}

void UAFLBattleRoyaleComponent::ServerStartMatch()
{
	if (!HasAuth() || bMatchStarted)
	{
		return;
	}

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_BR: cannot START -- no GameState. Aborting (retry once it exists)."));
		return;   // abort WITHOUT marking started -- a later call retries
	}

	// SOLO participants = every PlayerState present at Playing entry (human + bots; pawns are spawned by now).
	TotalParticipants = GS->PlayerArray.Num();
	if (TotalParticipants < 2)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_BR: starting with %d participant(s) -- last-standing is degenerate below 2."), TotalParticipants);
	}
	NextPlacement = TotalParticipants;

	bMatchStarted = true;
	MatchId = FGuid::NewGuid();   // authored ONCE, past the guard -> stable staking/earn contract id
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR_MATCHID assigned %s"), *GetMatchId());

	// ══ TAKE THE POT, HERE, AT THE ONE MOMENT THE ROSTER IS BOTH COMPLETE AND AUTHORITATIVE ═══════════════
	//
	// The match-start gate has already held Playing until every rostered human is present, so PlayerArray is
	// the real field rather than whoever arrived first. That is what makes this the right frame: escrow taken
	// earlier would miss a traveller, and taken later would charge people for a match already in progress.
	//
	// ONE UNIT PER PLAYER, because in a free-for-all each player IS a finishing position. EscrowFreeForAll
	// refuses the whole match rather than debiting anyone if bots are present (R85), if two players share a
	// runtime team (a squad, R92, which this path does not fund), or if anyone lacks a reconcile id.
	//
	// The ledger is held for the match lifetime -- see the member comment. Null here is not a failure: an
	// unstaked LEAGUE PLAY match has no pot, which is the ordinary case for every BR cell published today.
	EscrowLedger = FAFLMatchReporter::EscrowFreeForAll(this, MatchId, FAFLMatchReporter::ReadEconomics(this));

	Placements.Reset();
	DepartedParticipants.Reset();

	// FORFEIT TRIGGER. Bound at match start rather than in BeginPlay so a logout before the match exists is
	// simply a logout -- there is no ladder to take a rung from yet. Same event the bot fill converge uses.
	//
	// GUARDED: OnGameModeLogoutEvent is a GLOBAL multicast, and RestartMatch runs ServerStartMatch again on the
	// same component. Without this an in-session restart would book two rungs for one leaver.
	if (!bLogoutHookBound)
	{
		FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &UAFLBattleRoyaleComponent::HandlePlayerLoggedOut);
		bLogoutHookBound = true;
	}

	SetPhaseAuthoritative(EAFLBRPhase::Playing);
	AlivePlayers = AliveParticipants();
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: match START (participants=%d, alive=%d, survivorsToWin=%d, id=%s)."),
		TotalParticipants, AlivePlayers, SurvivorsToWin, *GetMatchId());

	// No-respawn for the whole match: dead is out. State.Round.NoRespawn on every PlayerState ASC (persists
	// across pawns) + ShouldBlockRestart() -> AAFLGameMode::ControllerCanRestart. Both, belt-and-braces.
	SetRespawnBlocked(true);

	// BR ends on last-standing, NOT the match-phase 480s clock -- take external match-end authority on the
	// resident match-phase component (present for the warmup->playing spine), leaving its cadence untouched.
	if (UAFLMatchPhaseComponent* MatchPhase = GS->FindComponentByClass<UAFLMatchPhaseComponent>())
	{
		MatchPhase->SetExternalMatchEndAuthority(true);
	}

	// Bind death on the now-live pawns (the per-possession join hook covers anyone arriving later).
	BindDeathDelegates();

	// INSTRUMENTATION (BLOCK 177): the FULL participant roster + team ids, logged ONCE at match start.
	LogBeliefState(TEXT("MATCH_START"), nullptr);
}

bool UAFLBattleRoyaleComponent::BookPlacement(APlayerState* PS)
{
	// ONE BOOKING SITE FOR BOTH TRIGGERS. Death and forfeit are the same ladder step and must not be able to
	// disagree about the rung or the double-book guard -- OnDeathStarted can fire twice for one pawn, and a
	// forfeit can arrive in the same frame as a death.
	if (!PS || Placements.Contains(PS))
	{
		return false;
	}
	Placements.Add(PS, NextPlacement);
	NextPlacement = FMath::Max(1, NextPlacement - 1);
	return true;
}

void UAFLBattleRoyaleComponent::HandlePlayerLoggedOut(AGameModeBase* /*GameMode*/, AController* Exiting)
{
	if (!HasAuth() || Phase != EAFLBRPhase::Playing)
	{
		return;   // before the match or after it concluded, leaving is just leaving
	}

	APlayerState* PS = Exiting ? Exiting->PlayerState : nullptr;
	if (!PS || PS->IsABot())
	{
		return;   // a bot leaving is bot fill doing its job, not a forfeit
	}

	// ⚠ THE PLAYERSTATE IS STILL IN PlayerArray DURING THIS BROADCAST and is destroyed immediately after --
	// AGameModeBase::Logout removes it once every listener has run. This is therefore the LAST frame in which
	// this player can be described at all, which is why the identity is captured here rather than referenced.
	const bool bBooked = BookPlacement(PS);
	if (!bBooked)
	{
		// Already dead and placed, then the client dropped. Their rung is correct and nothing more is owed.
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: %s disconnected after already being placed -- no forfeit needed."),
			*GetNameSafe(PS));
		return;
	}

	FAFLMatchParticipant Gone;
	Gone.bIsBot = false;
	Gone.TeamId = INDEX_NONE;                 // ruling: a free-for-all result has no teams economically
	Gone.FinishingPosition = Placements[PS];
	if (const UAFLReconcileIdComponent* IdComp = PS->FindComponentByClass<UAFLReconcileIdComponent>())
	{
		Gone.ReconcileId = IdComp->GetReconcileId();
	}
	if (Gone.ReconcileId.IsEmpty())
	{
		// LOUD. A staked forfeiter with no reconcile id cannot be settled, and the result will be refused at
		// report -- which now correctly leaves the teardown refund armed rather than stranding the pot.
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_BR: %s forfeited at placement %d but carries NO reconcile id -- they cannot be settled."),
			*GetNameSafe(PS), Gone.FinishingPosition);
	}
	DepartedParticipants.Add(MoveTemp(Gone));

	UE_LOG(LogAFLCombat, Warning,
		TEXT("AFL_BR: %s FORFEITED by disconnect -> placement %d. Stake stays escrowed; the match settles normally."),
		*GetNameSafe(PS), Placements[PS]);

	// ⚠ RE-CHECK THE END CONDITION. It is otherwise evaluated ONLY on a death, so a field that empties by
	// disconnects would run until the last two players killed each other -- or never. AliveParticipants reads
	// PlayerArray, which still holds this leaver for the rest of this frame, so the count is taken NEXT tick.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!HasAuth() || Phase != EAFLBRPhase::Playing) { return; }
			APlayerState* LastAlive = nullptr;
			AlivePlayers = AliveParticipants(&LastAlive);
			LogBeliefState(TEXT("FORFEIT"), nullptr);
			if (AlivePlayers <= SurvivorsToWin)
			{
				Server_EndMatch(AlivePlayers == 1 ? LastAlive : nullptr);
			}
		}));
	}
}

void UAFLBattleRoyaleComponent::HandlePlayerDeath(AActor* OwningActor)
{
	if (!HasAuth() || Phase != EAFLBRPhase::Playing)
	{
#if !UE_BUILD_SHIPPING
		// BLOCK 177: an early-return in the end-condition path is a STATED cause, not silence.
		UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_BR_STATE: HandlePlayerDeath early-return (hasAuth=%s phase=%d, need Playing=%d) -- death NOT counted."),
			HasAuth() ? TEXT("true") : TEXT("false"), (int32)Phase, (int32)EAFLBRPhase::Playing);
#endif
		return;
	}

	// Resolve the victim's PlayerState (the stable participant identity across its dead pawn).
	APlayerState* VictimPS = nullptr;
	if (const APawn* VictimPawn = Cast<APawn>(OwningActor))
	{
		VictimPS = VictimPawn->GetPlayerState();
	}

	// Book the finishing place (guarded: OnDeathStarted must count each participant once).
	if (BookPlacement(VictimPS))
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: %s eliminated -> placement %d."), *GetNameSafe(VictimPS), Placements[VictimPS]);
	}

	// Per-kill spatial telemetry (solo -> team INDEX_NONE), reusing the proven combat telemetry sink.
	const FVector Loc = OwningActor ? OwningActor->GetActorLocation() : FVector::ZeroVector;
	FAFLCombatTelemetry::EmitElimination(OwningActor, /*Killer=*/nullptr, /*VictimTeam=*/INDEX_NONE, Loc);

	// Recompute alive authoritatively (the just-dead pawn already reads IsDeadOrDying).
	APlayerState* LastAlive = nullptr;
	AlivePlayers = AliveParticipants(&LastAlive);

	// INSTRUMENTATION (BLOCK 177): state the FULL belief on every elimination so a stall is a READ, not an inference.
	LogBeliefState(TEXT("ELIMINATION"), VictimPS);

	if (AlivePlayers <= SurvivorsToWin)
	{
		// Last-standing (1 survivor) -> that PlayerState wins (placement 1); 0 survivors -> draw (null winner).
		Server_EndMatch(AlivePlayers == 1 ? LastAlive : nullptr);
	}
#if !UE_BUILD_SHIPPING
	else
	{
		// BLOCK 177: make the NON-conclusion visible as a stated cause. The end condition is re-checked ONLY on
		// the next elimination (no timer/poll -- bCanEverTick=false), so if deaths stop above threshold, silence.
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR_STATE: NO CONCLUSION this elimination -- alive=%d > survivorsToWin=%d; next check is the NEXT death only (no timer/poll)."),
			AlivePlayers, SurvivorsToWin);
	}
#endif
}

void UAFLBattleRoyaleComponent::Server_EndMatch(APlayerState* Winner)
{
	UnbindDeathDelegates();

	if (Winner && !Placements.Contains(Winner))
	{
		Placements.Add(Winner, 1);   // sole survivor takes first place
	}
	WinnerPlayerId = Winner ? Winner->GetPlayerId() : INDEX_NONE;

	SetPhaseAuthoritative(EAFLBRPhase::MatchEnd);
	OnRep_Resolved();   // listen-host local broadcast (OnRep does not fire for the authority's own change)

	FAFLCombatTelemetry::EmitRoundResolved(/*Round=*/0, WinnerPlayerId, FName(TEXT("last_standing")));
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: MATCH END -- winner %s (playerId=%d), participants=%d -> concluding."),
		*GetNameSafe(Winner), WinnerPlayerId, TotalParticipants);

	// ══ REPORT IT. Until this landed, BR played and paid nothing. ═════════════════════════════════════════
	//
	// ORDER MATTERS: before ConcludeMatch, while PlayerArray is still populated. The builder enumerates live
	// players to pair them with their placements, and conclusion begins the teardown that empties it.
	//
	// The latch is shared with the cancel path so a pot moves exactly once.
	if (!bEconomySettled)
	{
		const FAFLMatchReporter::FMatchEconomics Econ = FAFLMatchReporter::ReadEconomics(this);
		FAFLMatchResult Result;
		FString BuildError;
		if (FAFLMatchReporter::BuildFieldResult(this, MatchId, Placements, DepartedParticipants, Econ, Result, BuildError))
		{
			// ⚠ LATCHED ON THE REPORT, NOT ON THE BUILD. They are different questions and the gap between them
			// strands money. A field that loses a player to a disconnect BUILDS perfectly well -- every
			// remaining PlayerArray member has a placement -- but the leaver took their rung with them, so the
			// positions are not dense and ReportMatchEnd refuses them one step later. Latching on the build set
			// the flag anyway, the settlement never posted, and the EndPlay backstop below was suppressed by
			// the very flag meant to guard it. One disconnect stranded the entire pot with no refund.
			//
			// Sends settle only when staked and rating only when rated; an unstaked LEAGUE PLAY field reports
			// nothing, returns TRUE, and correctly latches -- it has no pot for a backstop to refund.
			bEconomySettled = FAFLMatchReporter::ReportMatchEnd(this, Result, Econ.StakePerPosition, Econ.CurrencyCode);
			if (!bEconomySettled)
			{
				UE_LOG(LogAFLCombat, Error,
					TEXT("AFL_BR: match %s built a result but REPORTED NOTHING -- the pot is untouched and the teardown refund is armed."),
					*GetMatchId());
			}
		}
		else
		{
			// LOUD. A staked field that cannot build a result has a pot sitting in escrow with nothing to
			// settle it -- the cancel path is what recovers that, and it needs a human to notice.
			UE_LOG(LogAFLCombat, Error,
				TEXT("AFL_BR: match %s ended but the field result could NOT be built -- %s. Nothing settled, nothing rated%s."),
				*GetMatchId(), *BuildError,
				(EscrowLedger.IsValid() && EscrowLedger->IsStaked()) ? TEXT("; A POT REMAINS IN ESCROW") : TEXT(""));
		}
	}

	// Conclude via the PROVEN PostGame machinery on the resident match-phase component (freeze via
	// State.Match.Ended + PostGame + per-player Watts banner). Idempotent (bMatchEnded). Null-guarded.
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr)
	{
		if (UAFLMatchPhaseComponent* MatchPhase = GS->FindComponentByClass<UAFLMatchPhaseComponent>())
		{
			MatchPhase->ConcludeMatch();
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_BR: MATCH END but no UAFLMatchPhaseComponent resident -- conclusion (freeze/PostGame/Watts) SKIPPED."));
		}
	}
	// RESTART-DEGRADATION FIX: restore respawn at match end (mirrors UAFLRoundManagerComponent::Server_EndMatch,
	// which calls SetRoundRespawnSuppressed(false) here). Leaving State.Round.NoRespawn SET made an in-session
	// restart begin with carried-over DEAD participants (alive < TotalParticipants), and let the bot-fill over-add
	// across restarts (the 144-participant accumulation -- dead-not-respawnable pawns never count as alive, so the
	// fill keeps adding). Permadeath still holds DURING the match (SetRespawnBlocked(true) at ServerStartMatch);
	// this only frees the between-match reset / the next match. PostGame's freeze is an ability-block, not a
	// respawn-block, so restoring respawn here is safe.
	SetRespawnBlocked(false);
}

int32 UAFLBattleRoyaleComponent::AliveParticipants(APlayerState** OutLastAlive) const
{
	if (OutLastAlive) { *OutLastAlive = nullptr; }
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return 0;
	}
	int32 Count = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		const APawn* P = PS->GetPawn();
		if (!P) { continue; }
		const ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(P);
		if (HC && !HC->IsDeadOrDying())
		{
			++Count;
			if (OutLastAlive) { *OutLastAlive = PS; }
		}
	}
	return Count;
}

int32 UAFLBattleRoyaleComponent::GetPlacementForPlayer(const APlayerState* PS) const
{
	if (!PS) { return 0; }
	const int32* Found = Placements.Find(PS);
	return Found ? *Found : 0;
}

void UAFLBattleRoyaleComponent::LogBeliefState(const FString& Context, const APlayerState* JustEliminated) const
{
#if !UE_BUILD_SHIPPING
	// PURE INSTRUMENTATION (BLOCK 177) -- no state change. Recomputes alive the same way the end condition does,
	// then prints the summary + a per-participant roster with the runtime team id (the previously-unlogged field).
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	const ULyraTeamSubsystem* Teams = GetWorld() ? GetWorld()->GetSubsystem<ULyraTeamSubsystem>() : nullptr;

	const int32 Alive = AliveParticipants();
	const bool bEndConditionMet = (Alive <= SurvivorsToWin);
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_BR_STATE: [%s] eliminated=%s | alive=%d | survivorsToWin=%d | endConditionMet=%s | total=%d | phase=%d | teamSubsystem=%s"),
		*Context,
		JustEliminated ? *GetNameSafe(JustEliminated) : TEXT("--"),
		Alive, SurvivorsToWin, bEndConditionMet ? TEXT("true") : TEXT("false"),
		TotalParticipants, (int32)Phase, Teams ? TEXT("present") : TEXT("MISSING"));

	if (!GS)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR_STATE:   (no GameState -- roster unavailable)"));
		return;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		const APawn* P = PS->GetPawn();
		const ULyraHealthComponent* HC = P ? ULyraHealthComponent::FindHealthComponent(P) : nullptr;
		const bool bAlive = (HC && !HC->IsDeadOrDying());
		const int32 TeamId = Teams ? Teams->FindTeamFromObject(PS) : INDEX_NONE;   // load-bearing field: runtime team assignment
		const int32 Place = GetPlacementForPlayer(PS);
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR_STATE:   %s | alive=%s | teamId=%d | placement=%s"),
			*GetNameSafe(PS), bAlive ? TEXT("true") : TEXT("false"), TeamId,
			Place > 0 ? *FString::FromInt(Place) : TEXT("--"));
	}
#endif
}

void UAFLBattleRoyaleComponent::SetRespawnBlocked(bool bBlocked)
{
	// ORDERING INVARIANT (from the round manager): CACHE BEFORE SWEEPING. bRespawnBlocked is the source of
	// truth for every later joiner (site #4 has no live query) AND for ShouldBlockRestart -- set it first.
	bRespawnBlocked = bBlocked;

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return;
	}
	int32 Count = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
		{
			ASC->SetLooseGameplayTagCount(TAG_State_Round_NoRespawn_BR, bBlocked ? 1 : 0);
			++Count;
		}
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: respawn %s on %d player ASC(s) (State.Round.NoRespawn)."),
		bBlocked ? TEXT("BLOCKED") : TEXT("RESTORED"), Count);
}

bool UAFLBattleRoyaleComponent::BindDeathDelegateForPawn(APawn* Pawn)
{
	ULyraHealthComponent* HC = Pawn ? ULyraHealthComponent::FindHealthComponent(Pawn) : nullptr;
	if (!HC || BoundHealthComps.Contains(HC))
	{
		return false;   // THE GUARD -- AddDynamic is not idempotent; a double bind double-counts a death.
	}
	HC->OnDeathStarted.AddDynamic(this, &UAFLBattleRoyaleComponent::HandlePlayerDeath);
	BoundHealthComps.Add(HC);
	return true;
}

void UAFLBattleRoyaleComponent::BindDeathDelegates()
{
	// Full reconcile at match start: drop stale weak entries + guarantee coverage even for a pawn possessed
	// before this component's BeginPlay. The per-possession join hook (ApplyJoinStateToPawn) covers arrivals.
	UnbindDeathDelegates();
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return;
	}
	int32 Bound = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (PS && BindDeathDelegateForPawn(PS->GetPawn()))
		{
			++Bound;
		}
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: death delegates reconciled -- %d pawn(s) bound of %d participant(s)."),
		Bound, GS->PlayerArray.Num());
}

void UAFLBattleRoyaleComponent::UnbindDeathDelegates()
{
	for (TWeakObjectPtr<ULyraHealthComponent>& Weak : BoundHealthComps)
	{
		if (ULyraHealthComponent* HC = Weak.Get())
		{
			HC->OnDeathStarted.RemoveDynamic(this, &UAFLBattleRoyaleComponent::HandlePlayerDeath);
		}
	}
	BoundHealthComps.Reset();
}

void UAFLBattleRoyaleComponent::ApplyJoinStateToPlayer(AController* NewPlayer, UAbilitySystemComponent* PlayerStateASC)
{
	if (!PlayerStateASC)
	{
		return;
	}
	// SITE #4 -- cached, not inferred. Set-count, not Add (may run after a sweep already covered this ASC).
	PlayerStateASC->SetLooseGameplayTagCount(TAG_State_Round_NoRespawn_BR, bRespawnBlocked ? 1 : 0);
}

void UAFLBattleRoyaleComponent::ApplyJoinStateToPawn(AController* Controller, APawn* NewPawn)
{
	// SITE #3 -- fires on join AND every respawn; binds a pawn created mid-match the moment it is possessed.
	if (BindDeathDelegateForPawn(NewPawn))
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: death delegate bound on %s (controller %s)."),
			*GetNameSafe(NewPawn), *GetNameSafe(Controller));
	}
}

void UAFLBattleRoyaleComponent::SetPhaseAuthoritative(EAFLBRPhase NewPhase)
{
	Phase = NewPhase;
	OnRep_Phase();   // OnRep does not fire for the authority's own change -> drive the listen-host locally
}

void UAFLBattleRoyaleComponent::OnRep_Phase()   { /* BlueprintReadOnly -- HUD reads Phase (may bind in a BP child). */ }
void UAFLBattleRoyaleComponent::OnRep_MatchId() { UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR_MATCHID replicated %s"), *GetMatchId()); }
void UAFLBattleRoyaleComponent::OnRep_Resolved(){ OnBattleRoyaleResolved.Broadcast(nullptr); /* winner identity via WinnerPlayerId; PS ptr not replicated */ }

#if !UE_BUILD_SHIPPING
// Dev trigger for the PIE watch (host-side authority world). Production trigger = the match-phase Playing entry.
static FAutoConsoleCommandWithWorld GAFLBRStartCmd(
	TEXT("afl.BR.Start"),
	TEXT("Start the Battle Royale FSM (ServerStartMatch on the authority GameState's BR component)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) { return; }
		if (AGameStateBase* GS = World->GetGameState())
		{
			if (UAFLBattleRoyaleComponent* BR = GS->FindComponentByClass<UAFLBattleRoyaleComponent>())
			{
				BR->ServerStartMatch();
			}
		}
	}));

// BLOCK 177: interrogate a LIVE stall on demand -- dumps the same belief state the elimination path logs, so the
// operator can read a stalled match rather than only its post-mortem. Conforms to afl.BR.Start above.
static FAutoConsoleCommandWithWorld GAFLBRDumpStateCmd(
	TEXT("afl.BR.DumpState"),
	TEXT("Dump the BR component's full belief state (alive/end-condition + per-participant roster w/ team ids) on demand."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) { return; }
		if (AGameStateBase* GS = World->GetGameState())
		{
			if (const UAFLBattleRoyaleComponent* BR = GS->FindComponentByClass<UAFLBattleRoyaleComponent>())
			{
				BR->LogBeliefState(TEXT("DUMPSTATE_CHEAT"), nullptr);
			}
		}
	}));
#endif

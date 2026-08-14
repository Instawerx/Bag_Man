// Copyright C12 AI Gaming. All Rights Reserved.

#include "Bots/AFLBotFillComponent.h"

#include "AFLGameCore.h"                       // LogAFLGameCore
#include "Teams/AFLMatchmakerDataProvider.h"   // roster count, tier-known test, AND AreBotsPermitted (the one policy)
#include "AIController.h"                       // AAIController (bot controllers)
#include "Teams/LyraTeamSubsystem.h"           // live team count + FindTeamFromObject
#include "GameModes/LyraGameMode.h"            // ALyraGameMode::OnGameModePlayerInitialized
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"        // FGameModeEvents (logout)
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"    // human filter (APlayerController)
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"            // GetIntOption (URL "NumBots" parity)
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"   // phase QUERY only, and reflectively -- see IsPhaseActiveReflected
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBotFillComponent)

// The two match phases the roster wait cares about. Their drivers are file-local to AFLMatchPhaseComponent
// (AFLCombat, a GameFeature this always-loaded core module cannot reference), so we define our own statics
// for the same strings -- UE dedups -- and observe WITHOUT linking that module's symbols. Same technique
// UAFLRoundManagerComponent and AAFLExtractionZone already use for this tag.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Playing_Bots, "AFL.GamePhase.Playing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_PostGame_Bots, "AFL.GamePhase.PostGame");

namespace
{
	// ULyraGamePhaseSubsystem::IsPhaseActive is public and BlueprintCallable and STILL does not link from
	// outside LyraGame -- the subsystem class carries no LYRAGAME_API, so none of its members cross the DLL
	// boundary, UFUNCTION or not. Reflection is the only route. Param layout = (FGameplayTag, bool return).
	struct FK2IsPhaseActiveParams
	{
		FGameplayTag PhaseTag;
		bool ReturnValue = false;
	};
}

#if WITH_SERVER_CODE

int32 UAFLBotFillComponent::GetNumTeams() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const ULyraTeamSubsystem* Teams = World->GetSubsystem<ULyraTeamSubsystem>())
		{
			return Teams->GetTeamIDs().Num();
		}
	}
	return 0;
}

int32 UAFLBotFillComponent::ComputeTargetTotal() const
{
	// Structural capacity: how many seats the authored team set can actually hold.
	const int32 Structural = FMath::Max(0, TeamSize) * GetNumTeams();

	// The playlist's declared bracket wins. GetAuthGameMode() rather than GetGameMode<>() so this stays const --
	// the converge pass calls it from a const context and MUST agree with the one-shot fill.
	const UWorld* World = GetWorld();
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (!GameMode)
	{
		return Structural;
	}

	const int32 Declared = UGameplayStatics::GetIntOption(GameMode->OptionsString, TEXT("FieldSize"), 0);
	if (Declared <= 0)
	{
		return Structural;   // no declaration -- every pre-existing mode keeps its exact behaviour
	}

	if (Declared > Structural)
	{
		// Seating more players than there are team slots doubles players up, which silently breaks solo BR.
		// Clamp, but say so loudly: this is an authoring error in the playlist or the team set, not a tuning knob.
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFLBots: playlist declares FieldSize=%d but the team set only seats %d (TeamSize=%d x %d teams) -- clamping."),
			Declared, Structural, TeamSize, GetNumTeams());
		return Structural;
	}

	return Declared;
}

int32 UAFLBotFillComponent::CountHumans() const
{
	int32 Humans = 0;
	const UWorld* World = GetWorld();
	if (const AGameStateBase* GameState = World ? World->GetGameState() : nullptr)
	{
		for (const APlayerState* PS : GameState->PlayerArray)
		{
			if (PS && !PS->IsABot() && !PS->IsOnlyASpectator())
			{
				++Humans;
			}
		}
	}
	return Humans;
}

bool UAFLBotFillComponent::IsTierKnown() const
{
	// A tier is trustworthy unless an external authority owns the roster and has not delivered it yet. With no
	// external roster at all (PIE / offline / a launch line stating its own ?Tier=) the launch options ARE the
	// answer, so there is nothing to wait for.
	return !UAFLMatchmakerDataProvider::IsRosterExternallyOwned(this)
		|| !UAFLMatchmakerDataProvider::ResolveAuthoritativeMatchmakerData(this).IsEmpty();
}

bool UAFLBotFillComponent::ShouldBarBots() const
{
	// DELEGATED, NOT REIMPLEMENTED. This function used to carry its own copy of the policy -- tier known?
	// tier permits? -- and the same policy was written a second time inside the provider's per-join gate and a
	// third time in the result validator. Two of those three drifted, and the second one was only caught
	// because the first was fixed and produced five teamless bots.
	//
	// So the policy now lives in exactly ONE function and everyone asks it. The behaviour here is unchanged:
	// UAFLMatchmakerDataProvider::AreBotsPermitted is the identical rule, fail-closed on an unknown tier.
	return !UAFLMatchmakerDataProvider::AreBotsPermitted(this);
}

int32 UAFLBotFillComponent::ResolveHumanBaseline() const
{
	// OPTION A, named in this file's header and in IAFLTeamAssignmentProvider since T1, implemented here.
	//
	// This is `UAFLMatchmakerDataProvider::GetExpectedHumanCount()`'s body verbatim rather than a call to it,
	// and deliberately: that method is non-static on the provider, and the provider may not EXIST yet at
	// fill time -- UAFLTeamCreationComponent::IsAssignmentAuthoritative() "deliberately does not construct
	// one". Going through the instance would make the bot count depend on who asked first. The statics reach
	// the same payload with no such ordering. (Consequence to know: a roster injected via the provider's test
	// setter does not reach the fill; only the real payload does.)
	const int32 Expected = UAFLMatchmakerDataProvider::CountRosterMembers(
		UAFLMatchmakerDataProvider::ResolveAuthoritativeMatchmakerData(this));
	if (Expected >= 0)
	{
		return Expected;   // INDEX_NONE means NO ROSTER -- never a roster of zero. Only >= 0 is an answer.
	}

	// No roster: PIE / offline / LocalFill. Humans arrive on their own schedule and are only ever observed.
	return CountHumans();
}

bool UAFLBotFillComponent::IsPhaseActiveReflected(const UWorld* World, const FGameplayTag& PhaseTag)
{
	const ULyraGamePhaseSubsystem* Sub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(World);
	if (!Sub) { return false; }
	if (UFunction* Fn = Sub->FindFunction(TEXT("IsPhaseActive")))
	{
		FK2IsPhaseActiveParams Params;
		Params.PhaseTag = PhaseTag;
		const_cast<ULyraGamePhaseSubsystem*>(Sub)->ProcessEvent(Fn, &Params);
		return Params.ReturnValue;
	}
	return false;
}

void UAFLBotFillComponent::TickRosterWait()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FTimerManager& Timers = World->GetTimerManager();

	// ── (1) PLAYING BEGAN. CHECKED FIRST, AND IT BEATS A ROSTER THAT ARRIVED THIS SAME TICK. ──────────────
	//
	// Bots materialising mid-match is visible and unexplainable to the players in it; being short-handed at
	// least looks like what it is. So this is a REFUSAL, not a late fill.
	//
	// Reaching it also means an invariant broke. UAFLMatchPhaseComponent holds Warmup->Playing until the
	// payload has arrived AND a human is present (871d0eea), so under the healthy flow the roster is ALWAYS
	// here before Playing and this branch is unreachable. Hence Error, not Log: the correct response to a
	// broken invariant is a loud record of it, never a silent recovery that hides which invariant broke.
	if (IsPhaseActiveReflected(World, TAG_AFL_GamePhase_Playing_Bots))
	{
		Timers.ClearTimer(RosterWaitTimer);
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFLBots: roster wait ABANDONED after %.0fs -- AFL.GamePhase.Playing is already active and bots "
			     "are NOT filled mid-match. The match-phase gate should have held Playing until the payload "
			     "arrived, so reaching this means that gate was bypassed or the payload came late. Playing "
			     "SHORT-HANDED (target %d, %d human(s) present) rather than spawning bots into a live match."),
			RosterWaitedSeconds, ComputeTargetTotal(), CountHumans());
		return;
	}

	// ── (2) The match concluded without ever reaching Playing -- a NO SHOW. Nothing to fill, and nobody to
	// fill it for. Quiet: this is an ordinary outcome, not a fault.
	if (IsPhaseActiveReflected(World, TAG_AFL_GamePhase_PostGame_Bots))
	{
		Timers.ClearTimer(RosterWaitTimer);
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFLBots: roster wait ENDED after %.0fs -- the match concluded before Playing (no show)."),
			RosterWaitedSeconds);
		return;
	}

	// ── (3) THE THING WE WERE WAITING FOR. The tier is now answerable, so re-run the decision that could not
	// be made at experience load.
	if (IsTierKnown())
	{
		Timers.ClearTimer(RosterWaitTimer);

		if (ShouldBarBots())
		{
			// Arrived, and it bars bots. Terminal -- the wait is over and the answer is no.
			UE_LOG(LogAFLGameCore, Log,
				TEXT("AFLBots: roster arrived after %.0fs and its tier BARS bots (staked/rated) -- no fill, as "
				     "intended. R74/R85."), RosterWaitedSeconds);
			return;
		}

		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFLBots: roster arrived after %.0fs -- re-evaluating the fill that stood down at experience "
			     "load (expected humans %d, target %d)."),
			RosterWaitedSeconds, ResolveHumanBaseline(), ComputeTargetTotal());

		// DIRECTLY, not through the join hook. That is what makes this one mechanism cover both hazards: it
		// re-fires when humans joined BEFORE the payload (hazard 1) and it does not care whether the converge
		// hooks ever bound (hazard 2).
		ReconcileBotFill();
		return;
	}

	// ── (4) THE BOUND. See RosterWaitBudgetSeconds: past the GameLift queue timeout no payload can arrive.
	RosterWaitedSeconds += RosterPollSeconds;
	if (RosterWaitedSeconds >= RosterWaitBudgetSeconds)
	{
		Timers.ClearTimer(RosterWaitTimer);
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFLBots: roster wait EXPIRED after %.0fs with no payload, no Playing phase and no conclusion. "
			     "The placement was cancelled at source (the queue timeout) -- there is nothing left to wait "
			     "for, and this process is holding a session nobody can join."), RosterWaitedSeconds);
	}
}

#endif // WITH_SERVER_CODE

void UAFLBotFillComponent::ServerCreateBots_Implementation()
{
#if WITH_SERVER_CODE
	if (BotControllerClass == nullptr)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFLBots: no BotControllerClass set; skipping bot fill."));
		return;
	}

	// Reset the name pool exactly as stock does before spawning.
	RemainingBotNames = RandomBotNames;

	const int32 NumTeams = GetNumTeams();       // team creation ran HighPriority, before this LowPriority pass
	const int32 Target = ComputeTargetTotal();  // playlist FieldSize, else TeamSize * NumTeams (3 * 2 = 6 for 3v3)

	// OPTION A: subtract the humans this match will SEAT, not the humans who have arrived. At experience load
	// the present count is 0 for every placed match -- everyone is still travelling -- so the old
	// `Target - CountHumans()` asked for a FULL field of bots and relied on converge to take them back out
	// one join at a time. The roster answers the question directly and does not oscillate.
	const int32 HumanBaseline = ResolveHumanBaseline();
	int32 EffectiveBotCount = FMath::Max(0, Target - HumanBaseline);

	// Keep parity with stock's URL override so QA can still force an exact count.
	if (AGameModeBase* GameMode = GetGameMode<AGameModeBase>())
	{
		EffectiveBotCount = UGameplayStatics::GetIntOption(GameMode->OptionsString, TEXT("NumBots"), EffectiveBotCount);
	}

	// THE TIER GATE, same predicate as converge below (they must never disagree -- a fill that spawns and a
	// converge that removes is how bots get created and then stranded).
	//
	// Deliberately placed AFTER the ?NumBots= override so it also overrides it: bots are barred outright from a
	// staked or rated roster (R74/R85), so this is not a count to be tuned. The provider already refuses them a
	// team (255) and escrow already refuses the match; this stops them being created in the first place, which
	// is the only one of the three that keeps the match playable.
	//
	// EXPECT THIS TO FIRE ON EVERY GAMELIFT PLACEMENT, and not because the tier bars bots. The payload lands
	// ~4s after this pass (measured), so ShouldBarBots() is still in its fail-closed window here and the
	// one-shot fill can never be the thing that seats bots in a placed match. Converge does that, on the first
	// human join, by which time the tier is known. That asymmetry is intended, not a gap to be closed by
	// weakening the gate.
	if (EffectiveBotCount > 0 && ShouldBarBots())
	{
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFLBots: fill STANDS DOWN -- %d bot(s) suppressed; bots are barred for this match (staked/rated "
			     "tier, or an externally-owned roster that has not named its tier yet)."), EffectiveBotCount);
		EffectiveBotCount = 0;

		// ── ARM THE ROSTER WAIT ── but ONLY for the "not yet" reason.
		//
		// The two stand-down reasons are not the same thing and must not be treated as one. "This tier bars
		// bots" is a FINAL answer -- a staked match will never permit them, and arming a timer to re-ask would
		// poll for ten minutes to reach the conclusion it already has. "The tier has not arrived" is the only
		// one worth waiting on, and it is the reason this branch fires on every GameLift placement.
		if (!IsTierKnown() && !GetWorld()->GetTimerManager().IsTimerActive(RosterWaitTimer))
		{
			RosterWaitedSeconds = 0.f;
			GetWorld()->GetTimerManager().SetTimer(RosterWaitTimer, this,
				&UAFLBotFillComponent::TickRosterWait, RosterPollSeconds, /*bLoop=*/true);
			UE_LOG(LogAFLGameCore, Log,
				TEXT("AFLBots: roster wait ARMED (%.0fs poll, %.0fs bound) -- the tier is unknown until the "
				     "payload lands. Nothing keeps calling bot fill, so it must call itself."),
				RosterPollSeconds, RosterWaitBudgetSeconds);
		}
	}

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLBots: human-aware fill -- TeamSize=%d NumTeams=%d ExpectedHumans=%d (%s) -> %d bot(s) (target %d)"),
		TeamSize, NumTeams, HumanBaseline,
		UAFLMatchmakerDataProvider::ResolveAuthoritativeMatchmakerData(this).IsEmpty() ? TEXT("present-count, no roster") : TEXT("payload roster"),
		EffectiveBotCount, Target);

	// Reuse the stock spawn/possess/team-routing path unchanged -- each bot routes through
	// OnGameModePlayerInitialized -> ServerChooseTeamForPlayer -> the provider's balance.
	for (int32 Count = 0; Count < EffectiveBotCount; ++Count)
	{
		SpawnOneBot();
	}

	// --- Converge (displace/re-fill), SEAM-GATED (SSOT §0.2/§3) --------------------------------------------
	// The fill above counts humans PRESENT at experience-load; on a listen server only the host is connected
	// then, so it overshoots when remote clients join. While the active provider is NON-authoritative (LocalFill
	// ── CONVERGE HOOKS BIND UNCONDITIONALLY ──────────────────────────────────────────────────────────────
	//
	// This used to bind only when UAFLTeamCreationComponent::IsAssignmentAuthoritative() was FALSE -- i.e. it
	// decided "should converge ever run?" by asking who owns the roster. That was the third form of the same
	// confusion 1193fef1 removed from bot creation and the provider's per-join gate: an authoritative roster
	// does not mean bots are barred, it means a matchmaker chose the humans. A LEAGUE PLAY placement is
	// authoritative AND permits bots, and under the old gate its converge never bound at all.
	//
	// ⚠ AND IT MUST NOT BE "FIXED" BY SWAPPING IN AreBotsPermitted() HERE. That would reintroduce the exact
	// hazard this file already documents: bind time is EXPERIENCE LOAD, which under GameLift is ~4s BEFORE the
	// payload lands, so the tier is unknown, the fail-closed answer is "barred", and the hooks would never bind
	// -- permanently, on a match that turns out to permit bots. The old gate failed for precisely this reason
	// ("reads authority ONCE, at experience load"). Asking a FIRE-TIME question at BIND time is the bug; asking
	// a different fire-time question at bind time would just be the same bug with a better predicate.
	//
	// So bind always and decide late. ReconcileBotFill() opens with ShouldBarBots(), evaluated when the hook
	// actually fires -- by which point the payload has landed and the tier is knowable. Binding costs two
	// delegate subscriptions; being unable to converge for the life of the match costs the match. In a barred
	// match the hooks still bind, fire, and stand down with a log, which is correct and cheap.
	if (!bConvergeHooksBound)
	{
		if (ALyraGameMode* GameMode = GetGameMode<ALyraGameMode>())
		{
			GameMode->OnGameModePlayerInitialized.AddUObject(this, &UAFLBotFillComponent::HandlePlayerJoined);
		}
		FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &UAFLBotFillComponent::HandlePlayerLoggedOut);
		bConvergeHooksBound = true;
	}
#endif // WITH_SERVER_CODE
}

#if WITH_SERVER_CODE

void UAFLBotFillComponent::HandlePlayerJoined(AGameModeBase* /*GameMode*/, AController* NewPlayer)
{
	// Bots fire this hook too -- react to HUMANS only (else our own SpawnOneBot would recurse). Humans possess
	// an APlayerController; bots an AAIController.
	//
	// ⚠ THIS RUNS BEFORE TEAM-CREATION'S HANDLER, not after. An earlier version of this comment claimed the
	// opposite -- that team-creation, "bound HighPriority, before ours", had already assigned the joining
	// human's team. Both handlers bind to OnGameModePlayerInitialized with a plain AddUObject and there is no
	// priority on that delegate; team-creation merely binds FIRST, and UE's native multicast Broadcast() walks
	// its invocation list in REVERSE registration order, so binding first means firing last.
	//
	// The 2026-08-09 acceptance log shows it directly: converge logged its result before the joining human's
	// team assignment, and the provider was not selected until the FIRST BOT WE SPAWNED joined and asked for it.
	//
	// The old comment's conclusion still happened to hold -- CountHumans() counts PlayerArray membership and a
	// joining human's PlayerState is present by now, regardless of team -- but the stated reason was wrong, and
	// the guard in ReconcileBotFill exists because reasoning like it produced a full field of bots in a staked
	// match. Do not reintroduce an assumption that team assignment has already happened here.
	if (!NewPlayer || !NewPlayer->IsA(APlayerController::StaticClass()))
	{
		return;
	}
	ReconcileBotFill();
}

void UAFLBotFillComponent::HandlePlayerLoggedOut(AGameModeBase* GameMode, AController* Exiting)
{
	// FGameModeEvents is a PROCESS-GLOBAL multicast -- in multi-world PIE it fires for every world, so ignore
	// logouts that are not from OUR world's authoritative game mode.
	UWorld* World = GetWorld();
	if (!World || GameMode != World->GetAuthGameMode())
	{
		return;
	}
	// HUMANS only (removing a bot must not trigger a re-fill loop). The leaving PlayerState is still in
	// PlayerArray during the logout broadcast, so reconcile NEXT tick once the count reflects the departure.
	if (!Exiting || !Exiting->IsA(APlayerController::StaticClass()))
	{
		return;
	}
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]
	{
		ReconcileBotFill();
	}));
}

void UAFLBotFillComponent::ReconcileBotFill()
{
	if (bReconciling)
	{
		return;
	}

	// S12 GATE: never fill seats on a roster this server does not own.
	//
	// The bind gate in the fill pass reads authority ONCE, at experience load. Under GameLift the payload has
	// not landed then, so the provider is the provisional LocalFill, it reads non-authoritative, and the hooks
	// bind permanently. They then fire on a match whose sides were settled before anyone connected.
	//
	// MEASURED, acceptance run 2026-08-09 (all inside one 7 s hitch frame, [452]):
	//     11.07.37  one-shot fill -- Humans=0 -> 0 bot(s)      <- correct, ?NumBots=0 honoured
	//     11.08.31  onStartGameSession -- 263 bytes            <- roster arrives, 54 s later
	//     11.09.05  converge fires on human 1's join, Provider still NULL, Desired = 4-1 = 3
	//     11.08.58  bot 'Hubert' joins -- and HIS join is what first calls GetProvider()
	//     11.09.05  bots 'Eliza','Tinplate' join; converge logs "3 bot(s)"
	//     11.09.05  human 2 joins -> converge trims to 2
	//     11.12.43  escrow REFUSES: 2 bot(s) in a staked match (R85). Nobody debited.
	//
	// Two things this proves, both of which the obvious fix would have missed. First, converge runs BEFORE any
	// provider object exists, so UAFLTeamCreationComponent::IsAssignmentAuthoritative() -- which reports
	// `Provider && Provider->IsAuthoritative()` and deliberately does not construct one -- returns false here
	// and would have made this guard a no-op. Second, the bots were not a side effect of the roster arriving;
	// converge would have spawned them anyway, and the first bot's own join is what selected the provider.
	//
	// So ask the TIER, not who has already built a provider. ShouldBarBots() is the same predicate the one-shot
	// fill uses, and it still treats a roster whose payload is in flight as barring bots -- a seat that is
	// about to be claimed is not an empty seat, and a tier nobody has stated yet is not a permitting one.
	//
	// THIS IS NOW THE PATH THAT ACTUALLY SEATS BOTS IN A PLACED MATCH. The one-shot fill runs before the
	// payload and can only ever stand down; converge runs on the first human join, when the roster is present,
	// the tier is known and ResolveHumanBaseline() can answer from it. A solo LEAGUE PLAY commit therefore
	// fills on that human's join: target 6, expected 1, five bots.
	//
	// NOTE the asymmetry this leaves deliberately: the one-shot fill honours ?NumBots= and converge never has
	// (it targets ComputeTargetTotal() - ResolveHumanBaseline() outright), which is why ?NumBots=0 suppressed
	// the fill and bots still appeared. A local match that wants a hard bot cap still cannot get one from
	// converge; that is a separate gap, not this bug.
	if (ShouldBarBots())
	{
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFLBots: converge STANDS DOWN -- bots are barred for this match (staked/rated tier, or an "
			     "externally-owned roster that has not named its tier yet)."));
		return;
	}

	bReconciling = true;

	// OPTION A here too -- the two passes MUST use the same baseline or they fight: a one-shot fill sized
	// against the roster and a converge sized against present humans would spawn on every arrival and trim on
	// the next. With the roster, DesiredBots is constant for the whole match and each join is a no-op.
	const int32 DesiredBots = FMath::Max(0, ComputeTargetTotal() - ResolveHumanBaseline());

	// Trim overflow from the fuller team (holds the balanced split); backfill the floor with stock spawns.
	// The safety bound guards against any pathological churn (never expected -- Target is small and fixed).
	int32 Safety = 64;
	while (SpawnedBotList.Num() > DesiredBots && SpawnedBotList.Num() > 0 && Safety-- > 0)
	{
		RemoveOneBotOnFullerTeam();
	}
	while (SpawnedBotList.Num() < DesiredBots && Safety-- > 0)
	{
		SpawnOneBot();
	}

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLBots: converge -- ExpectedHumans=%d (%s) Present=%d Target=%d -> %d bot(s)"),
		ResolveHumanBaseline(),
		UAFLMatchmakerDataProvider::ResolveAuthoritativeMatchmakerData(this).IsEmpty() ? TEXT("present-count, no roster") : TEXT("payload roster"),
		CountHumans(), ComputeTargetTotal(), SpawnedBotList.Num());

	bReconciling = false;
}

void UAFLBotFillComponent::RemoveOneBotOnFullerTeam()
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	ULyraTeamSubsystem* Teams = World ? World->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
	if (!GameState || !Teams || SpawnedBotList.Num() == 0)
	{
		return;
	}

	// Per-team member counts off the live registry (bot-safe -- the same key the provider balances on).
	TMap<int32, int32> Counts;
	for (const APlayerState* PS : GameState->PlayerArray)
	{
		if (PS)
		{
			++Counts.FindOrAdd(Teams->FindTeamFromObject(PS));
		}
	}

	// Fuller team = the highest live count.
	int32 FullerTeam = INDEX_NONE;
	int32 FullerCount = -1;
	for (const TPair<int32, int32>& Pair : Counts)
	{
		if (Pair.Value > FullerCount)
		{
			FullerCount = Pair.Value;
			FullerTeam = Pair.Key;
		}
	}

	// Prefer a bot standing on the fuller team; fall back to the last-spawned bot if none matches.
	AAIController* Victim = nullptr;
	for (int32 Index = SpawnedBotList.Num() - 1; Index >= 0; --Index)
	{
		AAIController* Bot = SpawnedBotList[Index];
		if (!Bot)
		{
			SpawnedBotList.RemoveAt(Index);
			continue;
		}
		if (Bot->PlayerState && Teams->FindTeamFromObject(Bot->PlayerState) == FullerTeam)
		{
			Victim = Bot;
			break;
		}
	}
	if (!Victim && SpawnedBotList.Num() > 0)
	{
		Victim = SpawnedBotList.Last();
	}
	if (!Victim)
	{
		return;
	}

	SpawnedBotList.Remove(Victim);

	// Clean roster trim (not a combat death): destroy the pawn then the controller. The controller's Logout
	// removes its PlayerState from PlayerArray so the live counts settle.
	if (APawn* Pawn = Victim->GetPawn())
	{
		Pawn->Destroy();
	}
	Victim->Destroy();
}

#endif // WITH_SERVER_CODE

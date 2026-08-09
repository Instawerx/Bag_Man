// Copyright C12 AI Gaming. All Rights Reserved.

#include "Bots/AFLBotFillComponent.h"

#include "AFLGameCore.h"                       // LogAFLGameCore
#include "Teams/AFLTeamCreationComponent.h"    // IsAssignmentAuthoritative (the bind-time seam gate)
#include "Teams/AFLMatchmakerDataProvider.h"   // S12: IsRosterExternallyOwned -- the fire-time gate on both fills
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
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBotFillComponent)

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
	const int32 HumanCount = CountHumans();     // humans PRESENT at experience-load -- converges below
	const int32 Target = ComputeTargetTotal();  // playlist FieldSize, else TeamSize * NumTeams (3 * 2 = 6 for 3v3)
	int32 EffectiveBotCount = FMath::Max(0, Target - HumanCount);

	// Keep parity with stock's URL override so QA can still force an exact count.
	if (AGameModeBase* GameMode = GetGameMode<AGameModeBase>())
	{
		EffectiveBotCount = UGameplayStatics::GetIntOption(GameMode->OptionsString, TEXT("NumBots"), EffectiveBotCount);
	}

	// S12 GATE, same rule as converge below: an externally-owned roster gets NO bots, and this pass is where
	// production would otherwise create them. It runs at experience load with Humans=0 -- every rostered player
	// is still connecting -- so `Target - Humans` is the FULL field. The acceptance run only escaped that
	// because it was launched with ?NumBots=0 by hand; a real placement carries no such option, so this pass
	// would have spawned a full field of bots into a staked match and, with converge now standing down, they
	// would have stayed. Gating only converge would have left production strictly worse than before.
	//
	// Deliberately placed AFTER the ?NumBots= override so it also overrides it: bots are barred outright from a
	// staked or rated roster (R74/R85), so this is not a count to be tuned. The provider already refuses them a
	// team (255) and escrow already refuses the match; this stops them being created in the first place, which
	// is the only one of the three that keeps the match playable.
	if (EffectiveBotCount > 0 && UAFLMatchmakerDataProvider::IsRosterExternallyOwned(this))
	{
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFLBots: fill STANDS DOWN -- %d bot(s) suppressed; the roster is externally owned and its "
			     "players are still connecting."), EffectiveBotCount);
		EffectiveBotCount = 0;
	}

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLBots: human-aware fill -- TeamSize=%d NumTeams=%d Humans=%d -> %d bot(s) (target %d)"),
		TeamSize, NumTeams, HumanCount, EffectiveBotCount, Target);

	// Reuse the stock spawn/possess/team-routing path unchanged -- each bot routes through
	// OnGameModePlayerInitialized -> ServerChooseTeamForPlayer -> the provider's balance.
	for (int32 Count = 0; Count < EffectiveBotCount; ++Count)
	{
		SpawnOneBot();
	}

	// --- Converge (displace/re-fill), SEAM-GATED (SSOT §0.2/§3) --------------------------------------------
	// The fill above counts humans PRESENT at experience-load; on a listen server only the host is connected
	// then, so it overshoots when remote clients join. While the active provider is NON-authoritative (LocalFill
	// / offline / PIE), converge to Target on each late HUMAN join/leave. A T2 MatchmakerDataProvider
	// (authoritative) seats all humans pre-start -> this path stays inert, and a future one-shot
	// `Target - IAFLTeamAssignmentProvider::GetExpectedHumanCount()` (Option A) replaces the present-count fill.
	bool bAuthoritative = false;
	const UWorld* World = GetWorld();
	if (const AGameStateBase* GameState = World ? World->GetGameState() : nullptr)
	{
		if (const UAFLTeamCreationComponent* TeamCreation = GameState->FindComponentByClass<UAFLTeamCreationComponent>())
		{
			bAuthoritative = TeamCreation->IsAssignmentAuthoritative();
		}
	}

	if (!bAuthoritative && !bConvergeHooksBound)
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
	// So ask who OWNS the roster, not who has already built a provider for it. IsRosterExternallyOwned answers
	// true both when the payload has arrived and while it is still in flight -- a seat that is about to be
	// claimed is not an empty seat.
	//
	// NOTE the asymmetry this leaves deliberately: the one-shot fill honours ?NumBots= and converge never has
	// (it targets ComputeTargetTotal() - CountHumans() outright), which is why ?NumBots=0 suppressed the fill
	// and bots still appeared. The guard makes that moot for externally-owned rosters. A local match that
	// wants a hard bot cap still cannot get one from converge; that is a separate gap, not this bug.
	if (UAFLMatchmakerDataProvider::IsRosterExternallyOwned(this))
	{
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFLBots: converge STANDS DOWN -- the roster is externally owned, so players who are absent "
			     "are rostered-but-still-connecting, not seats to fill."));
		return;
	}

	bReconciling = true;

	const int32 DesiredBots = FMath::Max(0, ComputeTargetTotal() - CountHumans());

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
		TEXT("AFLBots: converge -- Humans=%d Target=%d -> %d bot(s)"),
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

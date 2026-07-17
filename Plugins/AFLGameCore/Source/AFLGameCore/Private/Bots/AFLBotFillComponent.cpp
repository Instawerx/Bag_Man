// Copyright C12 AI Gaming. All Rights Reserved.

#include "Bots/AFLBotFillComponent.h"

#include "AFLGameCore.h"                       // LogAFLGameCore
#include "Teams/AFLTeamCreationComponent.h"    // IsAssignmentAuthoritative (the seam gate)
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
	return FMath::Max(0, TeamSize) * GetNumTeams();
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

	const int32 NumTeams = GetNumTeams();                     // team creation ran HighPriority, before this LowPriority pass
	const int32 HumanCount = CountHumans();                   // humans PRESENT at experience-load -- converges below
	const int32 Target = FMath::Max(0, TeamSize) * NumTeams;  // e.g. 3 * 2 = 6 for 3v3
	int32 EffectiveBotCount = FMath::Max(0, Target - HumanCount);

	// Keep parity with stock's URL override so QA can still force an exact count.
	if (AGameModeBase* GameMode = GetGameMode<AGameModeBase>())
	{
		EffectiveBotCount = UGameplayStatics::GetIntOption(GameMode->OptionsString, TEXT("NumBots"), EffectiveBotCount);
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
	// an APlayerController; bots an AAIController. Team-creation's handler (bound HighPriority, before ours) has
	// already assigned the joining human's team, so the live counts in ReconcileBotFill reflect the join.
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

// Copyright C12 AI Gaming. All Rights Reserved.

#include "Teams/AFLTeamCreationComponent.h"

#include "AFLGameCore.h"                    // LogAFLGameCore
#include "Teams/AFLLocalFillProvider.h"     // the active provider (LocalFill in T1)
#include "GameModes/LyraGameMode.h"         // ALyraGameMode::OnGameModePlayerInitialized
#include "Player/LyraPlayerState.h"         // ALyraPlayerState (SetGenericTeamId via ILyraTeamAgentInterface)
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLTeamCreationComponent)

#if WITH_SERVER_CODE

UAFLLocalFillProvider* UAFLTeamCreationComponent::GetProvider()
{
	if (!Provider)
	{
		Provider = NewObject<UAFLLocalFillProvider>(this);
	}
	return Provider;
}

void UAFLTeamCreationComponent::ServerAssignPlayersToTeams()
{
	// SSOT §1/§2 -- drive assignment through the swappable provider (LocalFill in T1). REAL PLAYERS ONLY in the
	// batch: this is the drop-in contract with the T2 MatchmakerDataProvider (bots are FILL, balanced per-join in
	// ServerChooseTeamForPlayer). The consumption layer (UAFLRoundManagerComponent) is untouched (§0.5).
	AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();

	TArray<APlayerController*> RealPlayers;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (!PS || PS->IsABot() || PS->IsOnlyASpectator())
		{
			continue;   // bots are FILL (per-join); spectators carry no team
		}
		if (APlayerController* PC = Cast<APlayerController>(PS->GetOwningController()))
		{
			RealPlayers.Add(PC);
		}
	}

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLTeams: UAFLTeamCreationComponent path -- LocalFill batch of %d real player(s)"), RealPlayers.Num());

	// Apply the provider's split index-parallel to RealPlayers (direct controller assignment -- which is exactly
	// why T1 sidesteps the T2 identity-join, §3). LocalFill fires OnReady synchronously, so the captured raw
	// controller pointers are valid for the duration of the call.
	FOnAFLTeamAssignmentsReady OnReady;
	OnReady.BindLambda([RealPlayers](const TArray<FAFLTeamAssignment>& Assignments)
	{
		const int32 Count = FMath::Min(RealPlayers.Num(), Assignments.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (APlayerController* PC = RealPlayers[Index])
			{
				if (ALyraPlayerState* LyraPS = PC->GetPlayerState<ALyraPlayerState>())
				{
					LyraPS->SetGenericTeamId(Assignments[Index].TeamId);   // ILyraTeamAgentInterface -> MyTeamID replication
				}
			}
		}
	});
	GetProvider()->RequestAssignments(RealPlayers, OnReady);

	// Late joiners (humans) AND bots flow through OnGameModePlayerInitialized -> our ServerChooseTeamForPlayer,
	// so per-join balance governs every future member. Registered here (not via Super) to keep a single assigner.
	if (ALyraGameMode* GameMode = Cast<ALyraGameMode>(GameState->AuthorityGameMode))
	{
		GameMode->OnGameModePlayerInitialized.AddUObject(this, &UAFLTeamCreationComponent::HandlePlayerInitialized);
	}
}

void UAFLTeamCreationComponent::HandlePlayerInitialized(AGameModeBase* /*GameMode*/, AController* NewPlayer)
{
	if (NewPlayer)
	{
		if (ALyraPlayerState* LyraPS = NewPlayer->GetPlayerState<ALyraPlayerState>())
		{
			ServerChooseTeamForPlayer(LyraPS);
		}
	}
}

void UAFLTeamCreationComponent::ServerChooseTeamForPlayer(ALyraPlayerState* PS)
{
	if (!PS)
	{
		return;
	}

	if (PS->IsOnlyASpectator())
	{
		PS->SetGenericTeamId(FGenericTeamId::NoTeam);   // preserve stock spectator behavior
		return;
	}

	// Per-join balance THROUGH the provider: live-count least-populated (bot-safe -- NO PlayerId cache; the v1
	// pile-up came from keying by an uninitialised PlayerId). Late humans AND bots route here.
	const FGenericTeamId TeamId = GetProvider()->ChooseBalancedTeam(this);
	PS->SetGenericTeamId(TeamId);

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLTeams: per-join balance -- %s '%s' -> team %d"),
		PS->IsABot() ? TEXT("BOT") : TEXT("player"),
		*PS->GetPlayerName(),
		static_cast<int32>(TeamId.GetId()));
}

#endif // WITH_SERVER_CODE

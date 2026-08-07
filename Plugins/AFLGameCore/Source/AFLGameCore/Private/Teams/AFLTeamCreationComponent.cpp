// Copyright C12 AI Gaming. All Rights Reserved.

#include "Teams/AFLTeamCreationComponent.h"

#include "AFLGameCore.h"                    // LogAFLGameCore
#include "Teams/AFLLocalFillProvider.h"        // offline / casual / PIE provider
#include "Teams/AFLMatchmakerDataProvider.h"   // authoritative provider -- reachable as of the Phase-0 unlock
#include "GameFramework/GameModeBase.h"        // OptionsString (the selection signal)
#include "Kismet/GameplayStatics.h"            // ParseOption
#include "GameModes/LyraGameMode.h"         // ALyraGameMode::OnGameModePlayerInitialized
#include "Player/LyraPlayerState.h"         // ALyraPlayerState (SetGenericTeamId via ILyraTeamAgentInterface)
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLTeamCreationComponent)

#if WITH_SERVER_CODE

IAFLTeamAssignmentProvider* UAFLTeamCreationComponent::GetProvider()
{
	if (!Provider)
	{
		// Selected ONCE and cached: a provider that could change mid-match would mean two different authorities
		// over the same roster, which is the drift hazard the single-assigner rule exists to prevent (SSOT §0.3).
		FString MatchmakerData;
		if (const AGameModeBase* GameMode = GetGameStateChecked<AGameStateBase>()->AuthorityGameMode)
		{
			MatchmakerData = UGameplayStatics::ParseOption(GameMode->OptionsString, TEXT("MatchmakerData"));
		}

		if (!MatchmakerData.IsEmpty())
		{
			Provider = NewObject<UAFLMatchmakerDataProvider>(this);
		}
		else
		{
			Provider = NewObject<UAFLLocalFillProvider>(this);
		}

		// Logged at Log, not Verbose. WHICH PROVIDER IS ACTIVE is the fact every downstream behaviour turns on --
		// the split, per-join resolution, and whether bot-fill's converge binds at all. Naming the concrete class
		// here is the evidence that was missing for as long as the seam was type-locked.
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFL_TEAMPROVIDER: selected %s (authoritative=%d) -- ?MatchmakerData= %s"),
			*GetNameSafe(Provider.GetObject()),
			Provider->IsAuthoritative() ? 1 : 0,
			MatchmakerData.IsEmpty() ? TEXT("ABSENT") : TEXT("PRESENT"));
	}
	return Provider ? Provider.GetInterface() : nullptr;
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
	if (IAFLTeamAssignmentProvider* ActiveProvider = GetProvider())
	{
		ActiveProvider->RequestAssignments(RealPlayers, OnReady);
	}

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

	// Per-join THROUGH the provider. The two providers answer different questions here -- LocalFill balances on
	// live counts (bot-safe: NO PlayerId cache, since the v1 pile-up came from keying by an uninitialised one),
	// the matchmaker looks the participant up in the roster. Late humans AND bots route through this one call.
	IAFLTeamAssignmentProvider* ActiveProvider = GetProvider();
	if (!ActiveProvider)
	{
		return;
	}

	const FGenericTeamId TeamId = ActiveProvider->ChooseTeamForJoiningPlayer(this, PS);
	PS->SetGenericTeamId(TeamId);

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLTeams: per-join -- %s '%s' -> team %d (provider %s)"),
		PS->IsABot() ? TEXT("BOT") : TEXT("player"),
		*PS->GetPlayerName(),
		static_cast<int32>(TeamId.GetId()),
		*GetNameSafe(Provider.GetObject()));
}

#endif // WITH_SERVER_CODE

bool UAFLTeamCreationComponent::IsAssignmentAuthoritative() const
{
	// Reports the ACTIVE provider's own answer. Before the Phase-0 unlock this could only ever be false, because
	// the field was typed to LocalFill and LocalFill's IsAuthoritative() is an inline `return false` -- so
	// UAFLBotFillComponent's converge gate was a dead branch. It is live now: with ?MatchmakerData= present the
	// authoritative provider is selected, this returns true, and bot-fill's converge stays unbound.
	//
	// Deliberately does NOT construct the provider: this is a query, and a const query that lazily creates state
	// would make "has a provider been selected yet" depend on who asked first.
	return Provider && Provider->IsAuthoritative();
}

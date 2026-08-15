// Copyright C12 AI Gaming. All Rights Reserved.

#include "Teams/AFLMatchmakerDataProvider.h"

#include "Online/AFLGameLiftHostSubsystem.h"   // S12: the GameLift-delivered roster, when it has arrived

#include "AFLGameCore.h"                       // LogAFLGameCore
#include "Match/AFLMatchReporter.h"            // ReadEconomics -- the ONE tier resolver
#include "Online/AFLLobbyTypes.h"              // AFLLobby::BotsPermitted -- the ONE bot policy
#include "Teams/AFLLocalFillProvider.h"        // ChooseBalancedTeam -- the ONE balance rule, reused for permitted bots
#include "Teams/AFLReconcileIdComponent.h"     // the per-player stashed reconcile key
#include "Teams/LyraTeamAgentInterface.h"      // IntegerToGenericTeamId
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"   // PlayerArray, for the already-present guard in TallyExpectedTeams
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"            // ParseOption (mirrors UAFLBotFillComponent's OptionsString read)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchmakerDataProvider)

TArray<FAFLTeamAssignment> UAFLMatchmakerDataProvider::ResolveAssignments(const FString& GameSessionDataJson,
	const TArray<FString>& OrderedReconcileIds)
{
	TArray<FAFLTeamAssignment> Out;
	Out.Reserve(OrderedReconcileIds.Num());

	// Parse the locked contract: { matchId, members: [ { id, type, team } ] } -> id -> AFL team id (the 0-based
	// roster index is mapped to the 1-based AFL team below).
	TMap<FString, int32> IdToTeam;
	FString MatchId;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GameSessionDataJson);
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
	{
		Root->TryGetStringField(TEXT("matchId"), MatchId);

		const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
		if (Root->TryGetArrayField(TEXT("members"), Members) && Members)
		{
			for (const TSharedPtr<FJsonValue>& MemberVal : *Members)
			{
				const TSharedPtr<FJsonObject> Member = MemberVal.IsValid() ? MemberVal->AsObject() : nullptr;
				if (!Member.IsValid())
				{
					continue;
				}
				FString Id;
				FString TeamStr;
				Member->TryGetStringField(TEXT("id"), Id);
				Member->TryGetStringField(TEXT("team"), TeamStr);
				if (!Id.IsEmpty())
				{
					// The roster team is a 0-BASED string index ("0"/"1"/...). Malformed -> 0.
					const int32 RosterIndex = TeamStr.IsNumeric() ? FCString::Atoi(*TeamStr) : 0;

					// >>> THE ONE TEAM-ID CONVENTION POINT <<<
					// The GameLift matchmaker roster indexes teams 0-BASED; the AFL team setup
					// (B_AFL_TeamSetup_TwoTeams -> TeamsToCreate {1,2}, confirmed 2026-07-17) is 1-BASED. Map the
					// 0-based roster index -> the 1-based AFL team id. N-TEAM GENERIC (roster "2" -> AFL team 3, ...).
					// If a real S12 roster ever arrives ALREADY 1-based, drop the +1 HERE only -- nothing else moves.
					const int32 AFLTeamId = RosterIndex + 1;

					IdToTeam.Add(Id, AFLTeamId);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFLTeams: Matchmaker ResolveAssignments -- GameSessionData is not valid JSON; roster empty."));
	}

	// Emit INDEX-PARALLEL to OrderedReconcileIds (the consumer applies Assignments[i] -> Players[i]); each id keeps
	// its OWN roster team regardless of connect order -> reconciliation is by id, not by position.
	for (const FString& ReconcileId : OrderedReconcileIds)
	{
		const int32* AFLTeamId = IdToTeam.Find(ReconcileId);
		const FGenericTeamId TeamId = AFLTeamId ? IntegerToGenericTeamId(*AFLTeamId) : FGenericTeamId::NoTeam;
		Out.Emplace(ReconcileId, TeamId);

		if (!AFLTeamId)
		{
			UE_LOG(LogAFLGameCore, Warning,
				TEXT("AFLTeams: Matchmaker -- reconcile id '%s' NOT in roster (match '%s') -> NoTeam."),
				*ReconcileId, *MatchId);
		}
	}

	return Out;
}

FString UAFLMatchmakerDataProvider::GetReconcileId(const APlayerController* PC)
{
	if (!PC || !PC->PlayerState)
	{
		return FString();
	}
	if (const UAFLReconcileIdComponent* IdComp = PC->PlayerState->FindComponentByClass<UAFLReconcileIdComponent>())
	{
		return IdComp->GetReconcileId();
	}
	return FString();
}

FString UAFLMatchmakerDataProvider::ResolveGameSessionData(const UObject* WorldContext) const
{
	if (!InjectedGameSessionData.IsEmpty())
	{
		return InjectedGameSessionData;   // the setter (unit tests) wins -- tests must be able to force a roster
	}
	return ResolveAuthoritativeMatchmakerData(WorldContext);
}

FString UAFLMatchmakerDataProvider::ResolveAuthoritativeMatchmakerData(const UObject* WorldContext)
{
	// S12: GameLift's onStartGameSession, held by UAFLGameLiftHostSubsystem. This is the PRODUCTION source.
	//
	// Consulted only when the payload has actually ARRIVED. GameLift delivers asynchronously, so "the
	// subsystem exists" is not the same question as "the roster is known" -- treating an empty payload as
	// authoritative would silently produce an unassigned-team match. Empty means "not mine to answer", and
	// the launch option below still gets its turn.
	if (const UAFLGameLiftHostSubsystem* GameLift = UAFLGameLiftHostSubsystem::Get(WorldContext))
	{
		if (GameLift->HasGameSessionData())
		{
			return GameLift->GetGameSessionData();
		}
	}

	// Fall back to the ?MatchmakerData= server launch option (the same OptionsString UAFLBotFillComponent reads
	// NumBots from). NOT removed by S12 -- it stays as the local/offline path, and it is what #20 was proven on.
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr)
	{
		return UGameplayStatics::ParseOption(GameMode->OptionsString, TEXT("MatchmakerData"));
	}
	return FString();
}

bool UAFLMatchmakerDataProvider::IsRosterExternallyOwned(const UObject* WorldContext)
{
	// Already delivered (either source) -- an external authority set these sides before anyone connected.
	if (!ResolveAuthoritativeMatchmakerData(WorldContext).IsEmpty())
	{
		return true;
	}

	// Still IN FLIGHT. The SDK being ready means this process was placed by GameLift, so a roster IS coming;
	// an empty payload right now is "not yet", not "never". Callers that fill empty seats must treat the two
	// the same way, because a seat that is about to be claimed is not an empty seat.
	if (const UAFLGameLiftHostSubsystem* GameLift = UAFLGameLiftHostSubsystem::Get(WorldContext))
	{
		if (GameLift->IsSdkReady() && !GameLift->HasGameSessionData())
		{
			return true;
		}
	}

	// No GameLift and no launch option -- offline / PIE / listen server. The roster is genuinely local.
	return false;
}

void UAFLMatchmakerDataProvider::RequestAssignments(const TArray<APlayerController*>& Players,
	const FOnAFLTeamAssignmentsReady& OnReady)
{
	// Ordered reconcile ids, index-parallel to Players: each controller's stashed PlayFab id.
	TArray<FString> OrderedReconcileIds;
	OrderedReconcileIds.Reserve(Players.Num());
	for (const APlayerController* PC : Players)
	{
		OrderedReconcileIds.Add(GetReconcileId(PC));
	}

	const UObject* Ctx = (Players.Num() > 0) ? static_cast<const UObject*>(Players[0]) : static_cast<const UObject*>(this);
	const FString GameSessionData = ResolveGameSessionData(Ctx);

	const TArray<FAFLTeamAssignment> Assignments = ResolveAssignments(GameSessionData, OrderedReconcileIds);

	for (const FAFLTeamAssignment& A : Assignments)
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFLTeams: Matchmaker split -- reconcile id '%s' -> team %d"),
			*A.PlayerId, static_cast<int32>(A.TeamId.GetId()));
	}

	OnReady.ExecuteIfBound(Assignments);
}

FString UAFLMatchmakerDataProvider::GetReconcileIdFromState(const APlayerState* PS)
{
	if (!PS)
	{
		return FString();
	}
	if (const UAFLReconcileIdComponent* IdComp = PS->FindComponentByClass<UAFLReconcileIdComponent>())
	{
		return IdComp->GetReconcileId();
	}
	return FString();
}

int32 UAFLMatchmakerDataProvider::CountRosterMembers(const FString& GameSessionDataJson)
{
	if (GameSessionDataJson.IsEmpty())
	{
		return INDEX_NONE;   // no roster is NOT a roster of zero -- the distinction is the whole point
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GameSessionDataJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return INDEX_NONE;
	}

	const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
	if (!Root->TryGetArrayField(TEXT("members"), Members) || !Members)
	{
		return INDEX_NONE;
	}
	return Members->Num();
}

int32 UAFLMatchmakerDataProvider::ReadFieldSize(const FString& GameSessionDataJson)
{
	if (GameSessionDataJson.IsEmpty())
	{
		return INDEX_NONE;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GameSessionDataJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return INDEX_NONE;
	}

	// ABSENT IS THE ORDINARY CASE, NOT A FAULT. The allocator omits the key when the ticket carried no
	// bracket -- every ticket minted before the attribute existed, and every non-matchmade launch. Silence
	// here is what makes the field additive: the caller falls back to the structural product unchanged.
	int32 FieldSize = 0;
	if (!Root->TryGetNumberField(TEXT("fieldSize"), FieldSize))
	{
		return INDEX_NONE;
	}

	// A declared field of zero or fewer is not a smaller match, it is a broken payload. Refuse it rather than
	// letting it read as "seat nobody" -- the fallback is the honest answer.
	return FieldSize > 0 ? FieldSize : INDEX_NONE;
}

void UAFLMatchmakerDataProvider::TallyExpectedTeams(const UObject* WorldContext,
	const FString& GameSessionDataJson, TMap<int32, int32>& OutCounts)
{
	OutCounts.Reset();
	if (GameSessionDataJson.IsEmpty())
	{
		return;   // no roster -> nothing is spoken for
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GameSessionDataJson);
	const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
		|| !Root->TryGetArrayField(TEXT("members"), Members) || !Members)
	{
		return;   // unparseable is NOT a roster -- same collapse CountRosterMembers makes
	}

	// ── THE DOUBLE-COUNT GUARD ───────────────────────────────────────────────────────────────────────────
	// A rostered human who has ALREADY arrived is in the live counts BuildLiveCounts produced. Seeding them
	// again would count one person twice and push bots off their side -- turning an over-correction into the
	// mirror image of the bug this fixes. So collect who is already here first, and seed only the absent.
	//
	// Keyed on GetReconcileIdFromState, which is the SAME key ChooseTeamForJoiningPlayer seats humans by. If
	// the two ever disagreed, a human could be both seeded and seated; sharing the key makes that impossible
	// rather than unlikely.
	TSet<FString> PresentReconcileIds;
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (const AGameStateBase* GameState = World ? World->GetGameState() : nullptr)
	{
		for (const APlayerState* PS : GameState->PlayerArray)
		{
			if (PS && !PS->IsABot())
			{
				const FString Key = GetReconcileIdFromState(PS);
				if (!Key.IsEmpty())
				{
					PresentReconcileIds.Add(Key);
				}
			}
		}
	}

	for (const TSharedPtr<FJsonValue>& MemberVal : *Members)
	{
		const TSharedPtr<FJsonObject> Member = MemberVal.IsValid() ? MemberVal->AsObject() : nullptr;
		if (!Member.IsValid())
		{
			continue;
		}
		FString Id, TeamStr;
		Member->TryGetStringField(TEXT("id"), Id);
		Member->TryGetStringField(TEXT("team"), TeamStr);
		if (Id.IsEmpty() || PresentReconcileIds.Contains(Id))
		{
			continue;   // already standing on the field -> already counted
		}

		// SAME 0-BASED -> 1-BASED MAPPING AS ResolveAssignments, deliberately mirrored rather than shared: that
		// function emits assignments index-parallel to a caller's id list, which is a different job. If the
		// roster ever arrives already 1-based, BOTH the +1 there and the +1 here move together or the balance
		// silently seeds the wrong side.
		const int32 RosterIndex = TeamStr.IsNumeric() ? FCString::Atoi(*TeamStr) : 0;
		OutCounts.FindOrAdd(RosterIndex + 1) += 1;
	}
}

bool UAFLMatchmakerDataProvider::AreBotsPermitted(const UObject* WorldContext)
{
	// ⚠ THE ONE PLACE THIS QUESTION IS ANSWERED. It was asked three separate times, three different ways, and
	// got the wrong answer twice -- bot CREATION (fixed in 1193fef1), bot ASSIGNMENT (the caller below, fixed
	// with this), and the result VALIDATOR (FAFLMatchResult::Validate, which had it right all along because it
	// tested bStaked||bRanked rather than authority). Three implementations of one policy is how two of them
	// drifted. There is now one, and the other sites call it.
	//
	// FAIL CLOSED ON AN UNKNOWN TIER. FAFLMatchReporter::ReadEconomics cannot say "I do not know": with no
	// payload it falls through to the launch line and answers LEAGUE PLAY, which PERMITS bots. So an
	// externally-owned roster that has not delivered yet must be treated as barring them -- a staked match
	// that briefly had bots cannot be un-had, while a permitted match that fills a moment later loses nothing.
	const bool bExternallyOwned = IsRosterExternallyOwned(WorldContext);
	const bool bTierIsKnown = !ResolveAuthoritativeMatchmakerData(WorldContext).IsEmpty();
	if (bExternallyOwned && !bTierIsKnown)
	{
		return false;
	}

	// Trustworthy now: the payload arrived (the tier the players actually matched on), or no external roster
	// exists at all (PIE / offline / a launch line stating its own ?Tier=).
	return AFLLobby::BotsPermitted(FAFLMatchReporter::ReadEconomics(WorldContext).Tier);
}

FGenericTeamId UAFLMatchmakerDataProvider::ChooseTeamForJoiningPlayer(const UObject* WorldContext,
	const APlayerState* JoiningPlayer) const
{
	if (!JoiningPlayer)
	{
		return FGenericTeamId::NoTeam;
	}

	// ── BOTS: THE TIER DECIDES, NOT THE AUTHORITY ────────────────────────────────────────────────────────
	//
	// This test used to be `IsABot()` alone, reached only on an authoritative roster -- i.e. it refused a bot
	// a team because SOMEONE ELSE OWNED THE ROSTER. That is the same tier-blind confusion 1193fef1 removed
	// from bot CREATION, left standing one layer down in bot ASSIGNMENT, and its own comment gave it away:
	// it cited "staked or rated" while the code asked "authoritative".
	//
	// MEASURED 2026-08-14, and it is exactly what a half-fix produces. With creation un-blinded, a solo LEAGUE
	// PLAY placement spawned the right five bots -- "ExpectedHumans=1 (payload roster) -> 5 bot(s) (target 6)"
	// -- and then all five landed here and were refused: five "-> team 255" lines, followed by
	// `Ensure condition failed: PlayerTeamId != INDEX_NONE` in AFLPlayerSpawningManagerComponent. Bots that
	// exist, have no side, and trip the spawner. Un-blinding one gate without the other is worse than neither.
	if (JoiningPlayer->IsABot())
	{
		if (!AreBotsPermitted(WorldContext))
		{
			// Barred: a STAKED or RATED roster (R74/R85, ai-bots §6.3), or a tier not yet knowable. Answering
			// NoTeam rather than balancing keeps such a bot VISIBLE as unassigned instead of quietly seated
			// into a staked side -- the original intent, now applied on the correct condition.
			UE_LOG(LogAFLGameCore, Warning,
				TEXT("AFLTeams: Matchmaker per-join -- BOT '%s' left unassigned; this match's tier does not "
				     "permit bots (staked/rated), or its tier is not yet known. ai-bots R74/R85."),
				*JoiningPlayer->GetPlayerName());
			return FGenericTeamId::NoTeam;
		}

		// PERMITTED. A bot has no roster entry, so the reconcile lookup below could never seat it -- balance it
		// instead, through LocalFill's rule so there is ONE balance rule rather than two that can drift. That
		// method is const, takes no participant and holds no instance state (both helpers behind it are
		// static, and the class has no members), so the CDO answers it without allocating. Deliberately NOT
		// keyed on the arriver's identity: bots arrive with an uninitialised PlayerId and keying on it piles
		// every bot onto one team -- the 2v3 trap LocalFill already documents.
		//
		// AND IT BALANCES AGAINST THE ROSTER, NOT JUST THE FIELD. Live counts alone answer "who is standing
		// here", which is the wrong question while rostered humans are still travelling: the bots would fill
		// first, split evenly among themselves, and each arriving human would then land on their ROSTERED side
		// and overload it. MEASURED 2026-08-14: solo LEAGUE PLAY roster, bots split 3/2, the human was seated
		// by roster onto the 3 side, and a 3v3 field ran 4v2 -- swept 7-0 by the heavy side.
		//
		// The COUNT was already roster-aware (Target - expected humans, 1193fef1); this makes the SIDES
		// roster-aware too. Same defect one layer over, and the same answer: ask the roster, not the arrivals.
		TMap<int32, int32> ExpectedByTeam;
		TallyExpectedTeams(WorldContext, ResolveGameSessionData(WorldContext), ExpectedByTeam);
		const FGenericTeamId BotTeam =
			GetDefault<UAFLLocalFillProvider>()->ChooseBalancedTeam(WorldContext, &ExpectedByTeam);
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFLTeams: Matchmaker per-join -- BOT '%s' -> team %d (balanced vs live + %d expected roster "
			     "seat(s); this tier permits bots)."),
			*JoiningPlayer->GetPlayerName(), static_cast<int32>(BotTeam.GetId()),
			[&ExpectedByTeam]{ int32 N = 0; for (const TPair<int32, int32>& P : ExpectedByTeam) { N += P.Value; } return N; }());
		return BotTeam;
	}

	const FString ReconcileId = GetReconcileIdFromState(JoiningPlayer);
	if (ReconcileId.IsEmpty())
	{
		// No key means ?PlayFabId= never arrived -- the connect-option append is still owed (Phase 3).
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFLTeams: Matchmaker per-join -- '%s' carries NO reconcile key -> NoTeam. The ?PlayFabId= "
			     "connect option is not being appended."),
			*JoiningPlayer->GetPlayerName());
		return FGenericTeamId::NoTeam;
	}

	const TArray<FAFLTeamAssignment> Resolved =
		ResolveAssignments(ResolveGameSessionData(WorldContext), TArray<FString>{ ReconcileId });

	const FGenericTeamId TeamId = Resolved.Num() > 0 ? Resolved[0].TeamId : FGenericTeamId::NoTeam;

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLTeams: Matchmaker per-join -- '%s' (key '%s') -> team %d"),
		*JoiningPlayer->GetPlayerName(), *ReconcileId, static_cast<int32>(TeamId.GetId()));

	return TeamId;
}

int32 UAFLMatchmakerDataProvider::GetExpectedHumanCount(const UObject* WorldContext) const
{
	return CountRosterMembers(ResolveGameSessionData(WorldContext));
}

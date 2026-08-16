// Copyright C12 AI Gaming. All Rights Reserved.

#include "Online/AFLGameLiftHostSubsystem.h"

#include "AFLGameCore.h"   // LogAFLGameCore
#include "Async/Async.h"                    // AsyncTask -- the websocket->game-thread hop for the travel
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectGlobals.h"         // FCoreUObjectDelegates::PostLoadMapWithWorld -- travel completion
#include "HAL/PlatformMisc.h"   // GetEnvironmentVariable -- Anywhere credentials, same pattern as AFLOnline
#include "Misc/CoreMisc.h"      // IsRunningDedicatedServer

// WITH_GAMELIFT is a PublicDefinition of the GameLiftServerSDK module, which AFLGameCore.Build.cs depends on
// for SERVER TARGETS ONLY. In an editor or client build the module is absent and the macro is never defined,
// so define it defensively rather than relying on `#if <undefined>` folding to 0 -- that is legal C++ but it
// hides a genuine "did the dependency actually get added" question behind a silent zero.
#ifndef WITH_GAMELIFT
#define WITH_GAMELIFT 0
#endif

#if WITH_GAMELIFT
#include "GameLiftServerSDK.h"
#include "GameLiftServerSDKModels.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameLiftHostSubsystem)

namespace
{
	/**
	 * Anywhere fleets require InitSDK to be given explicit connection parameters; a managed EC2 fleet can use
	 * the no-argument overload because the AMI supplies them. Our fleet is ANYWHERE (BagManTentpoleFleet), so
	 * these must come from somewhere -- and the environment is the same channel AFLOnline already uses for the
	 * earn HMAC key and endpoints, read once at startup. Keeping one convention avoids a second config story.
	 *
	 * Populated by RegisterCompute + GetComputeAuthToken at compute-registration time. That registration is a
	 * RUNTIME action taking the host's address, which is why the S12 plan recommends it live in a runbook
	 * rather than CloudFormation.
	 */
	constexpr const TCHAR* EnvWebSocketUrl = TEXT("AFL_GAMELIFT_WEBSOCKET_URL");
	constexpr const TCHAR* EnvAuthToken    = TEXT("AFL_GAMELIFT_AUTH_TOKEN");
	constexpr const TCHAR* EnvFleetId      = TEXT("AFL_GAMELIFT_FLEET_ID");
	constexpr const TCHAR* EnvHostId       = TEXT("AFL_GAMELIFT_HOST_ID");
	constexpr const TCHAR* EnvProcessId    = TEXT("AFL_GAMELIFT_PROCESS_ID");
}

UAFLGameLiftHostSubsystem* UAFLGameLiftHostSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UAFLGameLiftHostSubsystem>() : nullptr;
}

bool UAFLGameLiftHostSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Dedicated server only. A listen host or client has no GameLift process to report to, and creating this
	// there would imply the launch option had been superseded when it has not.
	return IsRunningDedicatedServer();
}

FString UAFLGameLiftHostSubsystem::GetGameSessionData() const
{
	FScopeLock Lock(&DataLock);
	return GameSessionDataJson;
}

bool UAFLGameLiftHostSubsystem::HasGameSessionData() const
{
	FScopeLock Lock(&DataLock);
	return !GameSessionDataJson.IsEmpty();
}

// Everything below to the #else uses GameLift SDK TYPES, which exist only on a server target -- the module
// is a server-only dependency and WITH_GAMELIFT is 0 everywhere else. The client build broke on exactly this
// omission: EPlayerSessionStatus and FGameLiftServerSDKModule are undeclared there, and the errors cascade
// from the first use rather than naming the real cause.
#if WITH_GAMELIFT

/**
 * S12-E RESEARCH PROBE (scope doc §4.2). Answers ONE question that the reconnect design depends on and that
 * cannot be answered from documentation with enough confidence to build on:
 *
 *     CAN A PLAYER SESSION THAT IS ALREADY `ACTIVE` BE ACCEPTED AGAIN BY A RETURNING CLIENT?
 *
 * The answer decides the whole reconnect path. If re-accept succeeds, a dropped player reconnects with the
 * id they already hold and the client change is a retry. If it fails, the backend must mint a fresh session
 * via CreatePlayerSession, which means a new endpoint AND a client-side reconnect flow -- a materially
 * bigger piece of work. Guessing wrong means building the wrong one.
 *
 * It also answers the follow-up: can a session be accepted after RemovePlayerSession, or is removal
 * terminal? That decides whether the grace window can safely release the slot early.
 *
 * ⚠ DESTRUCTIVE -- IT CONSUMES A REAL PLAYER SESSION. It accepts and then removes one of the placement's
 * sessions, so the player holding that id can no longer join. NEVER enable it for a real match; it exists
 * to be run against a throwaway placement with no humans connecting.
 *
 * Default OFF. Enable with -dpcvars=afl.GameLift.SessionProbe=1.
 */
static TAutoConsoleVariable<int32> CVarAFLGameLiftSessionProbe(
	TEXT("afl.GameLift.SessionProbe"),
	0,
	TEXT("S12-E research: on game-session activation, exercise the player-session lifecycle and log every ")
	TEXT("state transition. DESTRUCTIVE -- consumes a player session. Never enable for a real match."),
	ECVF_Default);

/** One describe, logged. Returns the status so the caller can assert transitions rather than eyeball them. */
static EPlayerSessionStatus ProbeDescribeOne(FGameLiftServerSDKModule& Mod, const FString& PlayerSessionId, const TCHAR* Stage)
{
	FGameLiftDescribePlayerSessionsRequest Req;
	Req.m_playerSessionId = PlayerSessionId;
	Req.m_limit = 1;

	const FGameLiftDescribePlayerSessionsOutcome Out = Mod.DescribePlayerSessions(Req);
	if (!Out.IsSuccess())
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_PROBE: [%s] describe FAILED -- %s"),
			Stage, *Out.GetError().m_errorMessage);
		return EPlayerSessionStatus::NOT_SET;
	}

	const TArray<FGameLiftPlayerSession>& Sessions = Out.GetResult().m_playerSessions;
	if (Sessions.Num() == 0)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_PROBE: [%s] describe returned NO sessions for %s"), Stage, *PlayerSessionId);
		return EPlayerSessionStatus::NOT_SET;
	}

	const EPlayerSessionStatus Status = Sessions[0].m_status;
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PROBE: [%s] %s  playerId='%s'  status=%s"),
		Stage, *PlayerSessionId, *Sessions[0].m_playerId, *GetNameForPlayerSessionStatus(Status));
	return Status;
}

/** Log an accept/remove attempt with its outcome -- the success/failure IS the research result. */
static bool ProbeAttempt(const FGameLiftGenericOutcome& Outcome, const TCHAR* What)
{
	if (Outcome.IsSuccess())
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PROBE: %s -> SUCCESS"), What);
		return true;
	}
	UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_PROBE: %s -> FAILED: %s"), What, *Outcome.GetError().m_errorMessage);
	return false;
}

static void RunPlayerSessionProbe(FGameLiftServerSDKModule& Mod)
{
	UE_LOG(LogAFLGameCore, Warning,
		TEXT("AFL_PROBE: ===== S12-E player-session probe START (DESTRUCTIVE -- consumes a player session) ====="));

	const FGameLiftStringOutcome IdOut = Mod.GetGameSessionId();
	if (!IdOut.IsSuccess())
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_PROBE: GetGameSessionId FAILED -- %s"), *IdOut.GetError().m_errorMessage);
		return;
	}
	const FString GameSessionId = IdOut.GetResult();
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PROBE: gameSessionId=%s"), *GameSessionId);

	// Enumerate what the placement reserved. Establishes the BASELINE state before anything is touched --
	// without it a later ACTIVE proves nothing, because it could have been ACTIVE all along.
	//
	// ⚠ THIS POLLS, AND THE DELAY IT MEASURES IS ITSELF A DESIGN INPUT -- it is not a retry papering over a
	// flake. First run of this probe enumerated once at activation and got ZERO sessions, while the AWS CLI
	// showed both existing moments later. So the placement's player sessions are NOT guaranteed visible to
	// the server SDK at the instant onStartGameSession fires.
	//
	// That gap matters for the real implementation: if a client can connect before its own session is
	// queryable here, a PreLogin that calls AcceptPlayerSession would reject a legitimate player. Measuring
	// how long visibility actually takes is the only way to know whether that race is reachable.
	TArray<FGameLiftPlayerSession> Sessions;
	const double StartedAt = FPlatformTime::Seconds();
	for (int32 Attempt = 1; Attempt <= 20; ++Attempt)
	{
		FGameLiftDescribePlayerSessionsRequest All;
		All.m_gameSessionId = GameSessionId;
		All.m_limit = 20;

		const FGameLiftDescribePlayerSessionsOutcome AllOut = Mod.DescribePlayerSessions(All);
		if (!AllOut.IsSuccess())
		{
			UE_LOG(LogAFLGameCore, Error, TEXT("AFL_PROBE: enumerate FAILED (attempt %d) -- %s"),
				Attempt, *AllOut.GetError().m_errorMessage);
			return;
		}

		Sessions = AllOut.GetResult().m_playerSessions;
		if (Sessions.Num() > 0)
		{
			UE_LOG(LogAFLGameCore, Log,
				TEXT("AFL_PROBE: player sessions became visible after %.2fs (attempt %d) -- %d session(s):"),
				FPlatformTime::Seconds() - StartedAt, Attempt, Sessions.Num());
			break;
		}
		FPlatformProcess::Sleep(0.5f);   // websocket callback thread, not the game thread -- safe to block here
	}

	for (const FGameLiftPlayerSession& S : Sessions)
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PROBE:   %s  playerId='%s'  status=%s"),
			*S.m_playerSessionId, *S.m_playerId, *GetNameForPlayerSessionStatus(S.m_status));
	}
	if (Sessions.Num() == 0)
	{
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_PROBE: NO player sessions visible after %.2fs of polling. Either the placement created "
			     "none, or server-SDK visibility is slower than the probe waits."),
			FPlatformTime::Seconds() - StartedAt);
		return;
	}

	const FString Target = Sessions[0].m_playerSessionId;
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PROBE: target = %s"), *Target);

	// Q0 baseline -> expect RESERVED
	ProbeDescribeOne(Mod, Target, TEXT("0/baseline"));

	// Q1 first accept -> expect SUCCESS, then ACTIVE
	ProbeAttempt(Mod.AcceptPlayerSession(Target), TEXT("1/accept #1"));
	ProbeDescribeOne(Mod, Target, TEXT("1/after accept #1"));

	// Q2 ***THE QUESTION***: accept an ALREADY-ACTIVE session. Success here means a reconnecting client can
	// simply re-present the id it already has; failure means the backend must mint a new one.
	ProbeAttempt(Mod.AcceptPlayerSession(Target), TEXT("2/accept #2 (ALREADY ACTIVE -- the reconnect question)"));
	ProbeDescribeOne(Mod, Target, TEXT("2/after accept #2"));

	// Q3 removal -> expect COMPLETED
	ProbeAttempt(Mod.RemovePlayerSession(Target), TEXT("3/remove"));
	ProbeDescribeOne(Mod, Target, TEXT("3/after remove"));

	// Q4 accept after removal. Decides whether a grace window may release the slot early and still let the
	// player back, or whether removal is the point of no return.
	ProbeAttempt(Mod.AcceptPlayerSession(Target), TEXT("4/accept after remove (is removal terminal?)"));
	ProbeDescribeOne(Mod, Target, TEXT("4/after accept-post-remove"));

	UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_PROBE: ===== S12-E player-session probe END ====="));
}

UAFLGameLiftHostSubsystem::EPlayerSessionCheck UAFLGameLiftHostSubsystem::CheckAndAcceptPlayerSession(
	const FString& PlayerSessionId, FString& OutPlayerId) const
{
	OutPlayerId.Reset();
	if (!bSdkReady || PlayerSessionId.IsEmpty())
	{
		return EPlayerSessionCheck::Rejected;
	}

	FGameLiftServerSDKModule& Mod = FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));

	// RESOLVE FIRST, ACCEPT SECOND, and the order is load-bearing. Accepting before we know who this is would
	// consume the reservation for a session we might then decide is not ours -- and acceptance is not
	// reversible except by RemovePlayerSession, which is terminal.
	FGameLiftDescribePlayerSessionsRequest Req;
	Req.m_playerSessionId = PlayerSessionId;
	Req.m_limit = 1;

	const FGameLiftDescribePlayerSessionsOutcome Out = Mod.DescribePlayerSessions(Req);
	if (!Out.IsSuccess())
	{
		// The service answered with an error -- a malformed or unknown id lands here. That is a rejection,
		// distinct from "known but not yet visible" below.
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_PLAYERSESSION: describe refused '%s' -- %s"),
			*PlayerSessionId, *Out.GetError().m_errorMessage);
		return EPlayerSessionCheck::Rejected;
	}

	const TArray<FGameLiftPlayerSession>& Sessions = Out.GetResult().m_playerSessions;
	if (Sessions.Num() == 0)
	{
		// Known-unknown. Measured propagation is ~1.44s, so this is very likely a client that beat its own
		// session's visibility rather than a forgery. NOT a rejection on its own -- the caller retries.
		return EPlayerSessionCheck::NotVisibleYet;
	}

	const FGameLiftPlayerSession& Session = Sessions[0];

	// A session already COMPLETED/TIMEDOUT cannot be accepted -- GameLift refuses it outright, and letting
	// the caller discover that via a generic accept failure would lose the reason.
	if (Session.m_status == EPlayerSessionStatus::COMPLETED || Session.m_status == EPlayerSessionStatus::TIMEDOUT)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_PLAYERSESSION: '%s' is %s -- refusing."),
			*PlayerSessionId, *GetNameForPlayerSessionStatus(Session.m_status));
		return EPlayerSessionCheck::Rejected;
	}

	const FGameLiftGenericOutcome Accepted = Mod.AcceptPlayerSession(PlayerSessionId);
	if (!Accepted.IsSuccess())
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_PLAYERSESSION: accept refused '%s' -- %s"),
			*PlayerSessionId, *Accepted.GetError().m_errorMessage);
		return EPlayerSessionCheck::Rejected;
	}

	OutPlayerId = Session.m_playerId;
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PLAYERSESSION: accepted '%s' -> playerId '%s' (was %s)."),
		*PlayerSessionId, *OutPlayerId, *GetNameForPlayerSessionStatus(Session.m_status));
	return EPlayerSessionCheck::Valid;
}

void UAFLGameLiftHostSubsystem::ReleasePlayerSession(const FString& PlayerSessionId) const
{
	if (!bSdkReady || PlayerSessionId.IsEmpty())
	{
		return;
	}
	FGameLiftServerSDKModule& Mod = FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
	const FGameLiftGenericOutcome Removed = Mod.RemovePlayerSession(PlayerSessionId);
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PLAYERSESSION: release '%s' -> %s"),
		*PlayerSessionId, Removed.IsSuccess() ? TEXT("COMPLETED") : *Removed.GetError().m_errorMessage);
}

void UAFLGameLiftHostSubsystem::SetAcceptingPlayers(bool bAccepting) const
{
	if (!bSdkReady)
	{
		return;   // offline / PIE / launch-option -- no session to open or close
	}
	FGameLiftServerSDKModule& Mod = FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
	const EPlayerSessionCreationPolicy Policy =
		bAccepting ? EPlayerSessionCreationPolicy::ACCEPT_ALL : EPlayerSessionCreationPolicy::DENY_ALL;

	const FGameLiftGenericOutcome Out = Mod.UpdatePlayerSessionCreationPolicy(Policy);
	if (Out.IsSuccess())
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PLAYERSESSION: creation policy -> %s"),
			bAccepting ? TEXT("ACCEPT_ALL") : TEXT("DENY_ALL"));
	}
	else
	{
		// Logged at Warning, not Error: failing to CLOSE the session is a hardening gap, not a breach -- the
		// PreLogin identity gate still refuses anyone whose session is not on this match's roster.
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_PLAYERSESSION: creation policy change FAILED -- %s"),
			*Out.GetError().m_errorMessage);
	}
}

#else   // !WITH_GAMELIFT

// Client / editor builds. The header declares these unconditionally so callers need no #if of their own; the
// gate that calls them is guarded by IsSdkReady(), which is permanently false here, so these are unreachable
// rather than merely harmless. Rejected is still the honest return: with no session authority present, no
// session can be validated.
UAFLGameLiftHostSubsystem::EPlayerSessionCheck UAFLGameLiftHostSubsystem::CheckAndAcceptPlayerSession(
	const FString& /*PlayerSessionId*/, FString& OutPlayerId) const
{
	OutPlayerId.Reset();
	return EPlayerSessionCheck::Rejected;
}

void UAFLGameLiftHostSubsystem::ReleasePlayerSession(const FString& /*PlayerSessionId*/) const
{
}

void UAFLGameLiftHostSubsystem::SetAcceptingPlayers(bool /*bAccepting*/) const
{
}

#endif  // WITH_GAMELIFT

void UAFLGameLiftHostSubsystem::ActivateOnce(const TCHAR* Why)
{
	// ⚠ ONE ACTIVATION, WHATEVER ROUTE ARRIVES FIRST. Three paths end here -- travel completed, travel was not
	// possible, travel was issued but the map never loaded -- and GameLift treats a second ActivateGameSession
	// as an error on an already-active session. The flag is read and written on the game thread only.
	if (bGameSessionActivated)
	{
		return;
	}
	bGameSessionActivated = true;

#if WITH_GAMELIFT
	FGameLiftServerSDKModule& Mod = FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
	const FGameLiftGenericOutcome Activated = Mod.ActivateGameSession();
	if (Activated.IsSuccess())
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_GAMELIFT: ActivateGameSession OK (%s) -- players may now connect."), Why);
	}
	else
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_GAMELIFT: ActivateGameSession FAILED (%s) -- %s"),
			Why, *Activated.GetError().m_errorMessage);
	}

	// S12-E research probe. AFTER activation on purpose: player sessions are only meaningful once the session
	// is active, and running it earlier would measure the wrong state. Gated off by default; see the probe's
	// comment for why it must never run against a real match.
	if (CVarAFLGameLiftSessionProbe.GetValueOnAnyThread() != 0)
	{
		RunPlayerSessionProbe(Mod);
	}
#else
	// Editor and client builds have no SDK. The travel path above is still compiled and still exercised by a
	// -game run, which is how it gets tested without a fleet; only the activation is absent, because there is
	// nothing to activate against.
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_GAMELIFT: activation skipped (%s) -- no GameLift SDK in this build."), Why);
#endif
}

void UAFLGameLiftHostSubsystem::BeginMatchTravel(const FString& GameSessionDataJsonCopy)
{
	check(IsInGameThread());

	FString MapName, ExperienceName;
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GameSessionDataJsonCopy);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			Root->TryGetStringField(TEXT("map"), MapName);
			Root->TryGetStringField(TEXT("experience"), ExperienceName);
		}
	}

	// ⚠ NO MAP OR NO EXPERIENCE IS NOT AN ERROR, AND MUST NOT BECOME ONE. Thirteen published cells have no
	// experience authored yet, and an older allocator sends neither field; in both cases the correct behaviour
	// is the one this server had before travel existed -- stay put and activate, so the match still runs. It
	// runs WITHOUT escrow, which the allocator also warns about, but a match that plays unbilled beats a
	// server that refuses to activate and strands nine seated players.
	if (MapName.IsEmpty() || ExperienceName.IsEmpty())
	{
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_GAMELIFT: no travel target in GameSessionData (map='%s' experience='%s') -- staying on the "
			     "launch map. UAFLMatchReporter will NOT exist here, so escrow and settlement will not run."),
			*MapName, *ExperienceName);
		ActivateOnce(TEXT("no travel target"));
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_GAMELIFT: no world to travel -- activating in place."));
		ActivateOnce(TEXT("no world"));
		return;
	}

	// ⚠ ACTIVATE ON MAP-LOAD-COMPLETE, NOT HERE. ServerTravel is asynchronous: it returns having QUEUED the
	// load. Activating now would tell GameLift to admit players while the destination is still loading, and a
	// client that connected in that window would join the launch map and then be travelled out from under
	// itself mid-login -- the PreLogin/AcceptPlayerSession race the probe in this file measured.
	//
	// Bound BEFORE the travel is issued, because a cooked map already in memory can complete inside the call.
	TravelCompleteHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda(
		[this](UWorld* LoadedWorld)
		{
			if (TravelCompleteHandle.IsValid())
			{
				FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(TravelCompleteHandle);
				TravelCompleteHandle.Reset();
			}
			UE_LOG(LogAFLGameCore, Log, TEXT("AFL_GAMELIFT: travel complete -- world '%s'."),
				LoadedWorld ? *LoadedWorld->GetMapName() : TEXT("<null>"));
			ActivateOnce(TEXT("after travel"));
		});

	// `listen` so the dedicated server accepts connections on the destination map, and the experience rides as
	// a URL OPTION -- which is how Lyra selects one, and the only per-match channel that survives the travel.
	const FString TravelUrl = FString::Printf(TEXT("%s?listen?Experience=%s"), *MapName, *ExperienceName);
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_GAMELIFT: travelling to '%s' (activation deferred until it loads)."),
		*TravelUrl);

	if (!World->ServerTravel(TravelUrl, /*bAbsolute=*/true))
	{
		// The travel was REFUSED outright -- a bad map name is the likely cause. Do not leave the session
		// unactivated: GameLift would eventually reap the process and the players would be stranded with no
		// explanation. Activate where we are and say plainly that escrow will not run.
		if (TravelCompleteHandle.IsValid())
		{
			FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(TravelCompleteHandle);
			TravelCompleteHandle.Reset();
		}
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_GAMELIFT: ServerTravel REFUSED for '%s' -- activating on the launch map. Escrow and "
			     "settlement will NOT run for this match."), *TravelUrl);
		ActivateOnce(TEXT("travel refused"));
	}
}

void UAFLGameLiftHostSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if !WITH_GAMELIFT
	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFL_GAMELIFT: built without the Server SDK (WITH_GAMELIFT=0) -- ?MatchmakerData= remains the source."));
#else
	// Absent credentials is the NORMAL local case (the #20 dedicated-server run had none). Report it plainly
	// and leave the launch option in charge rather than half-initialising the SDK.
	FServerParameters Params;
	Params.m_webSocketUrl = FPlatformMisc::GetEnvironmentVariable(EnvWebSocketUrl);
	Params.m_authToken    = FPlatformMisc::GetEnvironmentVariable(EnvAuthToken);
	Params.m_fleetId      = FPlatformMisc::GetEnvironmentVariable(EnvFleetId);
	Params.m_hostId       = FPlatformMisc::GetEnvironmentVariable(EnvHostId);
	Params.m_processId    = FPlatformMisc::GetEnvironmentVariable(EnvProcessId);

	if (Params.m_webSocketUrl.IsEmpty() || Params.m_fleetId.IsEmpty() || Params.m_hostId.IsEmpty())
	{
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFL_GAMELIFT: no Anywhere credentials in the environment (%s / %s / %s) -- SDK NOT started. "
			     "?MatchmakerData= remains the source. This is expected for a local run."),
			EnvWebSocketUrl, EnvFleetId, EnvHostId);
		return;
	}

	// A blank processId is legal to GameLift but makes two processes on one host indistinguishable in the
	// console, so default it to something traceable rather than leaving it empty.
	if (Params.m_processId.IsEmpty())
	{
		Params.m_processId = FString::Printf(TEXT("bagman-%d"), FPlatformProcess::GetCurrentProcessId());
	}

	FGameLiftServerSDKModule& SDK = FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));

	// The auth token is a CREDENTIAL. Log the shape of the config, never the token itself -- the same rule
	// AFLOnline applies to the earn HMAC key (held/MISSING, never the value).
	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFL_GAMELIFT: InitSDK fleet=%s host=%s process=%s ws=%s authToken=%s"),
		*Params.m_fleetId, *Params.m_hostId, *Params.m_processId, *Params.m_webSocketUrl,
		Params.m_authToken.IsEmpty() ? TEXT("MISSING") : TEXT("held"));

	const FGameLiftGenericOutcome InitOutcome = SDK.InitSDK(Params);
	if (!InitOutcome.IsSuccess())
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_GAMELIFT: InitSDK FAILED -- %s. Falling back to ?MatchmakerData=."),
			*InitOutcome.GetError().m_errorMessage);
		return;
	}

	FProcessParameters ProcessParams;

	// THE HOP. GameLift hands us the payload here, on ITS websocket thread. Store it under the lock, then
	// ActivateGameSession -- in that order, because activation is what tells GameLift players may connect,
	// and a player arriving before the roster is stored is precisely the mis-teaming this design prevents.
	ProcessParams.OnStartGameSession.BindLambda(
		[this](Aws::GameLift::Server::Model::GameSession GameSession)
		{
			// NOTE the accessors return `const char*`, NOT std::string. GameSession.h carries both variants
			// gated on GAMELIFT_USE_STD, and this build compiles the char* one -- calling .c_str() here is a
			// compile error (C2228). Guard the null: an absent payload must become an empty FString, never a
			// crash in UTF8_TO_TCHAR.
			const char* RawJson = GameSession.GetGameSessionData();
			const FString Json = RawJson ? FString(UTF8_TO_TCHAR(RawJson)) : FString();
			{
				FScopeLock Lock(&DataLock);
				GameSessionDataJson = Json;
			}
			UE_LOG(LogAFLGameCore, Log,
				TEXT("AFL_GAMELIFT: onStartGameSession -- %d bytes of GameSessionData received."), Json.Len());

			// ⚠ THE HOP TO THE GAME THREAD. This lambda runs on the GameLift SDK's WEBSOCKET thread, and
			// ServerTravel touches the world -- calling it here is a data race with whatever the game thread
			// is doing, and the kind that survives a hundred runs and then corrupts one. Everything from the
			// travel decision onward happens in BeginMatchTravel, on the game thread.
			//
			// The payload is passed BY VALUE rather than re-read under the lock on the other side: the value
			// that arrived with THIS session is the one this travel must use, and a backfill's
			// OnUpdateGameSession can legitimately overwrite the member between the two.
			AsyncTask(ENamedThreads::GameThread, [this, Json]()
			{
				BeginMatchTravel(Json);
			});
		});

	// A backfill or property change re-delivers the session. Take the new payload: the roster is the thing
	// most likely to have changed, and a stale roster mis-teams a backfilled joiner.
	//
	// GetGameSession() is available because this build compiles the char* variant of UpdateGameSession. It is
	// absent from the GAMELIFT_USE_STD variant, so if that define is ever turned on this stops compiling --
	// which is the good failure, far better than silently losing roster refreshes.
	ProcessParams.OnUpdateGameSession.BindLambda(
		[this](Aws::GameLift::Server::Model::UpdateGameSession UpdatedSession)
		{
			const char* RawTicket = UpdatedSession.GetBackfillTicketId();
			const char* RawJson   = UpdatedSession.GetGameSession().GetGameSessionData();
			const FString TicketId = RawTicket ? FString(UTF8_TO_TCHAR(RawTicket)) : FString();
			const FString Json     = RawJson   ? FString(UTF8_TO_TCHAR(RawJson))   : FString();

			// Only overwrite on a non-empty payload. An empty update must not erase a roster we already hold.
			if (!Json.IsEmpty())
			{
				FScopeLock Lock(&DataLock);
				GameSessionDataJson = Json;
			}
			UE_LOG(LogAFLGameCore, Log,
				TEXT("AFL_GAMELIFT: onUpdateGameSession (backfillTicket='%s') -- roster %s (%d bytes)."),
				*TicketId, Json.IsEmpty() ? TEXT("UNCHANGED, empty payload") : TEXT("refreshed"), Json.Len());
		});

	// GameLift reclaims the process on a false/late health check, so this must stay cheap and never block.
	ProcessParams.OnHealthCheck.BindLambda([]() { return true; });

	ProcessParams.OnTerminate.BindLambda(
		[]()
		{
			UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_GAMELIFT: onProcessTerminate -- GameLift is reclaiming this process."));
			FGameLiftServerSDKModule& Mod = FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
			Mod.ProcessEnding();
			FPlatformMisc::RequestExit(false);
		});

	// The port GameLift will advertise to clients. It must be the port this process actually listens on, so
	// read the live URL rather than assuming a default -- a mismatch here produces sessions nobody can join.
	ProcessParams.port = GetGameInstance() && GetGameInstance()->GetWorld() && GetGameInstance()->GetWorld()->URL.Port > 0
		? GetGameInstance()->GetWorld()->URL.Port
		: FURL::UrlConfig.DefaultPort;

	const FGameLiftGenericOutcome ReadyOutcome = SDK.ProcessReady(ProcessParams);
	if (!ReadyOutcome.IsSuccess())
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_GAMELIFT: ProcessReady FAILED -- %s. Falling back to ?MatchmakerData=."),
			*ReadyOutcome.GetError().m_errorMessage);
		return;
	}

	bSdkReady = true;
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_GAMELIFT: ProcessReady OK on port %d -- awaiting onStartGameSession."),
		ProcessParams.port);
#endif // WITH_GAMELIFT
}

void UAFLGameLiftHostSubsystem::Deinitialize()
{
#if WITH_GAMELIFT
	if (bSdkReady)
	{
		// Tell GameLift we are going away rather than letting it discover a dead health check. Without this
		// the fleet holds the slot until the check times out, and placements queue behind a corpse.
		FGameLiftServerSDKModule& SDK = FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
		SDK.ProcessEnding();
		SDK.Destroy();
		bSdkReady = false;
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_GAMELIFT: ProcessEnding sent, SDK destroyed."));
	}
#endif
	Super::Deinitialize();
}

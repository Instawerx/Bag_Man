// Copyright C12 AI Gaming. All Rights Reserved.

#include "Online/AFLGameLiftHostSubsystem.h"

#include "AFLGameCore.h"   // LogAFLGameCore
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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
				TEXT("AFL_GAMELIFT: onStartGameSession -- %d bytes of GameSessionData received; activating."),
				Json.Len());

			FGameLiftServerSDKModule& Mod = FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
			const FGameLiftGenericOutcome Activated = Mod.ActivateGameSession();
			if (!Activated.IsSuccess())
			{
				UE_LOG(LogAFLGameCore, Error, TEXT("AFL_GAMELIFT: ActivateGameSession FAILED -- %s"),
					*Activated.GetError().m_errorMessage);
			}
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

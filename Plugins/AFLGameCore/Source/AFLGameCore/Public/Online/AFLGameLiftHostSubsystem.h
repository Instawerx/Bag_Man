// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "AFLGameLiftHostSubsystem.generated.h"

/**
 * UAFLGameLiftHostSubsystem  (S12 -- the GameLift delivery hop)
 *
 * Owns the GameLift Server SDK process lifecycle on a dedicated server and holds the GameSessionData that
 * GameLift delivers at onStartGameSession. It is the SOURCE half of the S12 swap; the CONSUMER half is
 * UAFLMatchmakerDataProvider::ResolveGameSessionData, which asks this subsystem before falling back to the
 * ?MatchmakerData= launch option.
 *
 * WHY A GAMEINSTANCE SUBSYSTEM RATHER THAN A COMPONENT.
 * GameLift delivers the payload ASYNCHRONOUSLY, on its own websocket thread, at a moment it chooses. The
 * launch option it replaces was on the command line and therefore present before the map even loaded. That
 * difference is the main correctness risk in S12: anything that reads the roster during InitGame or
 * InitNewPlayer could observe an empty string and silently fall back to unassigned teams -- the same shape as
 * the Lyra experience-timing trap (a GameFeature component's BeginPlay running before the experience is
 * loaded). A GameInstance subsystem outlives every world and experience on the server process, so the
 * payload cannot be destroyed by a travel or arrive "before" its holder exists. Ordering is then reduced to
 * one question -- has it arrived yet -- which HasGameSessionData() answers honestly.
 *
 * THE LAUNCH OPTION IS NOT REMOVED. `?MatchmakerData=` remains the local/offline path and is what #20 was
 * proven on. This is additive: if the SDK never initialises (no Anywhere credentials in the environment,
 * or an editor/client build where WITH_GAMELIFT is not even defined), every method here reports "nothing"
 * and the launch option stays in charge. A machine with no GameLift configuration behaves exactly as before.
 *
 * SERVER ONLY. ShouldCreateSubsystem refuses outside a dedicated server, and the SDK dependency itself is
 * added only for server targets in AFLGameCore.Build.cs -- so this compiles, but does nothing, elsewhere.
 */
UCLASS()
class AFLGAMECORE_API UAFLGameLiftHostSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	/** Resolve from any world context. Null off a dedicated server, or before the game instance exists. */
	static UAFLGameLiftHostSubsystem* Get(const UObject* WorldContext);

	/**
	 * The GameSessionData GameLift delivered, or EMPTY if it has not arrived (or never will, on a machine
	 * with no GameLift configuration). Callers MUST treat empty as "not mine to answer" and fall through --
	 * never as "unstaked" or "no roster", which is how a timing bug becomes a silently mis-teamed match.
	 * Safe from any thread.
	 */
	FString GetGameSessionData() const;

	/** True once GameLift has delivered a non-empty payload. Safe from any thread. */
	bool HasGameSessionData() const;

	/** True once InitSDK + ProcessReady both succeeded. False means the launch option is still in charge. */
	bool IsSdkReady() const { return bSdkReady; }

private:
	/** Guards GameSessionDataJson. onStartGameSession arrives on the SDK's websocket thread, NOT the game
	 *  thread, while readers are on the game thread -- so the string genuinely needs a lock rather than
	 *  relying on it being "just a pointer swap". */
	mutable FCriticalSection DataLock;

	/** The verbatim JSON from GameLift. Same contract the allocator emits and ?MatchmakerData= carries. */
	FString GameSessionDataJson;

	/** Set once InitSDK + ProcessReady succeed. Not atomic-guarded: written once during Initialize on the
	 *  game thread, before any websocket callback can fire. */
	bool bSdkReady = false;
};

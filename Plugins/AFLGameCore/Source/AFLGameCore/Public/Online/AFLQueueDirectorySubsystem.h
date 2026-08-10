// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Interfaces/IHttpRequest.h"
#include "Online/AFLLobbyTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "AFLQueueDirectorySubsystem.generated.h"

/** Where the directory is in its fetch. Drives the lobby's loading state and nothing else. */
UENUM(BlueprintType)
enum class EAFLQueueDirectoryState : uint8
{
	/** Never fetched. The lobby has no ladder to draw yet. */
	Idle,
	/** One or both requests in flight. */
	Fetching,
	/** A queue set is in hand. Population may still be absent -- see `bPopulationKnown`. */
	Ready,
	/** `/queues` failed. There is no ladder, and that is a different thing from an empty one. */
	Failed
};

DECLARE_MULTICAST_DELEGATE_OneParam(FAFLOnQueueDirectoryUpdated, EAFLQueueDirectoryState /*State*/);

/**
 * UAFLQueueDirectorySubsystem -- what can be entered, and how busy it is.
 *
 * Reads `GET /queues` and `GET /population`, joins them on `queueId`, and hands the lobby a single set of
 * `FAFLLobbyQueue`. That is the whole job.
 *
 * ══ WHY IT LIVES HERE AND NOT NEXT TO THE WIDGETS ═════════════════════════════════════════════════════
 *
 * `UAFLW_Lobby_Root` deliberately fetches nothing -- a widget that owns its HTTP cannot be tested without a
 * network. The obvious home would then be beside it in AFLCombat, and that would silently never run:
 * **AFLCombat is `ExplicitlyLoaded: true`**, so its module loads at GameFeature activation, and a
 * `UGameInstanceSubsystem` declared in a module that loads AFTER the GameInstance is never instantiated.
 * The failure mode is not a crash or a log line -- it is `GetSubsystem` returning null forever.
 *
 * AFLGameCore is always-loaded, already owns the queue vocabulary (`AFLLobbyTypes.h`, next to the tier and
 * league enums the match result carries), and already depends on AFLOnline for the API base URL. So the
 * transport sits here and the widgets consume it in the correct direction: GameFeature -> always-loaded.
 *
 * ══ TWO REQUESTS, AND THEY FAIL DIFFERENTLY ON PURPOSE ════════════════════════════════════════════════
 *
 * `/queues` is the LADDER -- which cells exist and which are published. Without it there is nothing to
 * draw, so its failure is `Failed`.
 *
 * `/population` is the READING on those cells. Without it the ladder still renders, every cell reading
 * `Unknown` -> *"Count unavailable"*. That is not a degraded mode bolted on; it is exactly what
 * `IRONICS_LEAGUE_DOOR_SPEC.md` §7 requires: *"Population unavailable: dots go neutral, counts read `Count
 * unavailable`. Never fabricate a number here, and never render it as `0` -- 'we could not find out' and
 * 'nobody is there' are opposite claims."*
 *
 * ⚠ THE LADDER MUST BE FETCHED WITH `includeUnpublished=true`. Without it `/queues` returns published cells
 * only, and an unopened bracket would simply be absent -- but §3.2 rules that *"an unopened bracket is
 * still drawn. It is disabled rather than hidden, because the whole designed ladder is information."*
 * Hiding it would be honest but mute.
 *
 * ⚠ `mapPool` IS IN THE RESPONSE AND IS DELIBERATELY NOT PARSED. R18: the venue is a server outcome, and a
 * row that can name a map is one edit away from being a map browser. There is no field here to put it in,
 * which is the point.
 */
UCLASS()
class AFLGAMECORE_API UAFLQueueDirectorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~UGameInstanceSubsystem
	virtual void Deinitialize() override;
	//~End of UGameInstanceSubsystem

	/** Convenience accessor matching UAFLOnlineSubsystem::Get. Null outside a game instance. */
	static UAFLQueueDirectorySubsystem* Get(const UObject* WorldContextObject);

	/** Fire both reads. A refresh already in flight is not restarted -- the answer is seconds away. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby")
	void Refresh();

	/** The joined set, published and unpublished. Empty until the first successful `/queues`. */
	const TArray<FAFLLobbyQueue>& GetQueues() const { return Queues; }

	EAFLQueueDirectoryState GetState() const { return State; }

	/** False when the ladder is in hand but every cell reads `Unknown`. The lobby says so rather than lying. */
	bool IsPopulationKnown() const { return bPopulationKnown; }

	/** Fires on every settled refresh, including the population-failed one. */
	FAFLOnQueueDirectoryUpdated OnUpdated;

	/**
	 * Parse a `/queues` body into cells. Public and static so a test can hold the contract against a
	 * captured response with no network -- the same reason the row's formatters are static.
	 */
	static bool ParseQueuesResponse(const FString& Json, TArray<FAFLLobbyQueue>& OutQueues);

	/** Overlay a `/population` body onto an existing ladder. Returns how many cells it matched. */
	static int32 ApplyPopulationResponse(const FString& Json, TArray<FAFLLobbyQueue>& InOutQueues);

private:
	void HandleQueuesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void HandlePopulationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	/** Both legs have reported. Publish whatever we honestly have. */
	void SettleIfComplete();

	FString ResolveBaseUrl() const;

	UPROPERTY()
	TArray<FAFLLobbyQueue> Queues;

	/** Held until BOTH legs land, so a half-joined set is never published to the lobby. */
	TArray<FAFLLobbyQueue> PendingLadder;
	FString PendingPopulationBody;

	EAFLQueueDirectoryState State = EAFLQueueDirectoryState::Idle;

	bool bQueuesPending = false;
	bool bPopulationPending = false;
	bool bLadderOk = false;
	bool bPopulationOk = false;
	bool bPopulationKnown = false;
};

// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/OnlineReplStructs.h" // FUniqueNetIdRepl
#include "AFLChatTypes.h"

#include "AFLChatSubsystem.generated.h"

class IAFLChatTransport;
class IAFLChatFilter;
class UAFLChatComponent;
class FAFLServerReplicatedChatTransport;
struct FComponentRequestHandle;

/**
 * UAFLChatSubsystem -- the game-instance-level text-chat hub (COMMS-1). The UI's single subscription point
 * (OnChatMessage) and the single send entry (Send). Owns the transport registry (COMMS-1: one transport,
 * ServerReplicated), the ring-buffer history (200), and the content-filter seam (passthrough now).
 *
 * On Initialize it issues ONE UGameFrameworkComponentManager::AddComponentRequest attaching a
 * UAFLChatComponent to every APlayerController receiver (ALyraPlayerController is one -- no subclass), on
 * both server and client instances, and holds the request handle for its lifetime. The local player's
 * component registers back here (RegisterLocalComponent) so inbound is caught before the first send.
 */
UCLASS()
class AFLONLINE_API UAFLChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Resolve from any world-context object (null before the game instance exists). */
	static UAFLChatSubsystem* Get(const UObject* WorldContext);

	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Send a message on a channel. TargetId is meaningful only for Whisper. Runs the outbound filter, then
	 *  hands the (server-unstamped) message to the active transport; the server stamps identity + fans out. */
	void Send(EAFLChatChannel Channel, const FString& Body, const FUniqueNetIdRepl& TargetId = FUniqueNetIdRepl());

	/** UI's single subscription point -- fired for every message this client should display. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FAFLOnChatMessage, const FAFLChatMessage& /*Message*/);
	FAFLOnChatMessage OnChatMessage;

	/** Ring-buffer history (oldest-first), for a UI that opens mid-session. */
	const TArray<FAFLChatMessage>& GetHistory() const { return History; }

	/** The local component calls this on BeginPlay so the transport binds its inbound path. */
	void RegisterLocalComponent(UAFLChatComponent* Component);

	/** Called by the active transport on inbound: run the inbound filter, push to history, broadcast. */
	void HandleInbound(const FAFLChatMessage& Message);

private:
	static constexpr int32 HistoryCapacity = 200;

	/** First transport reporting IsAvailable() (COMMS-1: ServerReplicated). */
	IAFLChatTransport* GetActiveTransport() const;

	TSharedPtr<FAFLServerReplicatedChatTransport> ServerTransport;
	TArray<TSharedPtr<IAFLChatTransport>> Transports;
	TSharedPtr<IAFLChatFilter> Filter;
	FDelegateHandle InboundSubscription;

	TArray<FAFLChatMessage> History;

	/** Held for the subsystem lifetime -- releasing it removes UAFLChatComponent from PCs. */
	TSharedPtr<FComponentRequestHandle> ChatComponentRequest;
};

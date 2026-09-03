// Copyright C12 AI Gaming. All Rights Reserved.

#include "Chat/AFLChatSubsystem.h"

#include "Chat/AFLChatComponent.h"
#include "Chat/AFLChatFilter.h"
#include "Chat/AFLChatTransport.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLChatSubsystem)

/**
 * FAFLServerReplicatedChatTransport -- the COMMS-1 transport: outbound goes through the local player's
 * UAFLChatComponent::SubmitOutbound (server RPC), inbound arrives on that component's OnInbound (its
 * owner-only ClientReceiveChat) and is re-broadcast on OnMessageReceived, which the subsystem consumes.
 * Non-UObject; owned by the subsystem for its lifetime.
 */
class FAFLServerReplicatedChatTransport : public IAFLChatTransport
{
public:
	FAFLServerReplicatedChatTransport() = default;
	virtual ~FAFLServerReplicatedChatTransport() override
	{
		if (UAFLChatComponent* Comp = LocalComponent.Get())
		{
			Comp->OnInbound.RemoveAll(this);
		}
	}

	virtual FName GetTransportName() const override { return FName(TEXT("ServerReplicated")); }
	virtual bool IsAvailable() const override { return LocalComponent.IsValid(); }

	virtual void SendMessage(const FAFLChatMessage& Message) override
	{
		if (UAFLChatComponent* Comp = LocalComponent.Get())
		{
			Comp->SubmitOutbound(Message);
		}
	}

	/** Bind (or rebind, after travel) the local player's component to our inbound path. */
	void BindLocalComponent(UAFLChatComponent* Comp)
	{
		if (!Comp || LocalComponent.Get() == Comp) { return; }
		if (UAFLChatComponent* Old = LocalComponent.Get()) { Old->OnInbound.RemoveAll(this); }
		LocalComponent = Comp;
		Comp->OnInbound.AddRaw(this, &FAFLServerReplicatedChatTransport::HandleComponentInbound);
	}

private:
	void HandleComponentInbound(const FAFLChatMessage& Msg) { OnMessageReceived.Broadcast(Msg); }

	TWeakObjectPtr<UAFLChatComponent> LocalComponent;
};

// ------------------------------------------------------------------------------------------------

UAFLChatSubsystem* UAFLChatSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) { return nullptr; }
	const UWorld* World = WorldContext->GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? const_cast<UGameInstance*>(GI)->GetSubsystem<UAFLChatSubsystem>() : nullptr;
}

void UAFLChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Filter = MakeShared<FAFLPassthroughChatFilter>();

	ServerTransport = MakeShared<FAFLServerReplicatedChatTransport>();
	Transports.Add(ServerTransport);
	InboundSubscription = ServerTransport->OnMessageReceived.AddUObject(this, &UAFLChatSubsystem::HandleInbound);

	// Attach UAFLChatComponent to every APlayerController (ALyraPlayerController is a GameFramework receiver
	// -- ModularPlayerController.cpp:14). One request, held for the subsystem lifetime; the manager runs a
	// retroactive pass for already-spawned PCs and a live pass for future ones. Runs on BOTH server and
	// client instances (the server needs the component for the authoritative pipeline).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGameFrameworkComponentManager* Manager = GI->GetSubsystem<UGameFrameworkComponentManager>())
		{
			ChatComponentRequest = Manager->AddComponentRequest(
				TSoftClassPtr<AActor>(APlayerController::StaticClass()),
				UAFLChatComponent::StaticClass());
		}
	}
}

void UAFLChatSubsystem::Deinitialize()
{
	if (ServerTransport.IsValid() && InboundSubscription.IsValid())
	{
		ServerTransport->OnMessageReceived.Remove(InboundSubscription);
	}
	InboundSubscription.Reset();
	ChatComponentRequest.Reset(); // releasing the handle removes the component from PCs
	Transports.Reset();
	ServerTransport.Reset();
	Filter.Reset();
	History.Reset();

	Super::Deinitialize();
}

void UAFLChatSubsystem::RegisterLocalComponent(UAFLChatComponent* Component)
{
	if (ServerTransport.IsValid())
	{
		ServerTransport->BindLocalComponent(Component);
	}
}

IAFLChatTransport* UAFLChatSubsystem::GetActiveTransport() const
{
	for (const TSharedPtr<IAFLChatTransport>& T : Transports)
	{
		if (T.IsValid() && T->IsAvailable())
		{
			return T.Get();
		}
	}
	return nullptr;
}

void UAFLChatSubsystem::Send(EAFLChatChannel Channel, const FString& Body, const FUniqueNetIdRepl& TargetId)
{
	// System is server-originated ONLY. Refuse it here so a legitimate client never trips the server's
	// disconnect-grade Validate. (Party IS allowed through -- the server rejects it with a "reserved" reason,
	// which is a soft drop we deliberately exercise. A modified client that bypasses this guard and RPCs
	// System hits UAFLChatComponent::ServerSendChat_Validate -> connection closed.)
	if (Channel == EAFLChatChannel::System)
	{
		UE_LOG(LogAFLChat, Log, TEXT("AFL_CHAT[DROP_INVALID] client refused local Send(System) -- server-only channel"));
		return;
	}

	// Outbound filter (client-side; the server re-filters authoritatively).
	bool bBlock = false;
	FString FilteredBody = Body;
	if (Filter.IsValid())
	{
		FilteredBody = Filter->FilterOutbound(Body, bBlock);
	}
	if (bBlock) { return; }

	// Pre-trim for UX; the server is the authority and re-clamps.
	FilteredBody = UAFLChatComponent::SanitizeBody(FilteredBody);
	if (FilteredBody.IsEmpty()) { return; }

	IAFLChatTransport* Transport = GetActiveTransport();
	if (!Transport) { return; } // no local component yet / no connection

	FAFLChatMessage Msg;
	Msg.Channel = Channel;
	Msg.Body = FilteredBody;
	Msg.TargetId = TargetId;
	// SenderId / SenderDisplayName / ServerTimestamp are left empty -- the SERVER stamps them.
	Transport->SendMessage(Msg);
}

void UAFLChatSubsystem::HandleInbound(const FAFLChatMessage& Message)
{
	FAFLChatMessage Msg = Message;

	// Inbound filter (client-side display filter).
	bool bBlock = false;
	if (Filter.IsValid())
	{
		Msg.Body = Filter->FilterInbound(Msg.Body, bBlock);
	}
	if (bBlock) { return; }

	// Ring buffer (drop oldest past capacity).
	History.Add(Msg);
	if (History.Num() > HistoryCapacity)
	{
		History.RemoveAt(0, History.Num() - HistoryCapacity, EAllowShrinking::No);
	}

	OnChatMessage.Broadcast(Msg);
}

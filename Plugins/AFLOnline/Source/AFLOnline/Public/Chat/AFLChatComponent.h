// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "AFLChatTypes.h"

#include "AFLChatComponent.generated.h"

class IAFLChatFilter;
class APlayerController;
class APlayerState;

/** Structured server-side chat log: AFL_CHAT[SEND|DROP_RATE|DROP_INVALID|DROP_FILTER]. Rides the existing
 *  dedicated-server log pipeline; the subsystem shares this category. NEVER log a Whisper Body. */
AFLONLINE_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLChat, Log, All);

/**
 * UAFLChatComponent -- the per-PlayerController text-chat endpoint (COMMS-1). Attached to every
 * ALyraPlayerController by UAFLChatSubsystem via UGameFrameworkComponentManager::AddComponentRequest (no PC
 * subclass -- ALyraPlayerController is a GameFramework receiver, ModularPlayerController.cpp:14).
 *
 * The whole authoritative pipeline lives in ServerSendChat_Implementation (server only): auth check ->
 * channel/length validation -> control-char strip + 256 clamp -> per-player token bucket (burst 5, 1/s) ->
 * Party rejected (reserved) -> server-side filter seam -> recipient resolution (Say=all / Team=same-team /
 * Whisper=target+echo / System=server-only) -> per-recipient owner-only ClientReceiveChat. Team and Whisper
 * traffic is NEVER multicast -- privacy is server-side fan-out to exactly the intended owning connections.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLONLINE_API UAFLChatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLChatComponent();

	/** CLIENT entry point (owning client only): hand an outbound message to the server. The ServerReplicated
	 *  transport calls this; it stamps nothing (the server overwrites sender identity) and just invokes the RPC. */
	void SubmitOutbound(const FAFLChatMessage& Message);

	/** Fired on THIS client when a message is delivered here (from ClientReceiveChat). The transport binds this
	 *  and re-broadcasts to the subsystem; UI never touches the component directly. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FAFLOnComponentInbound, const FAFLChatMessage& /*Message*/);
	FAFLOnComponentInbound OnInbound;

	/** Server-originated System message fan-out (e.g. match events). Server-only; a client path never reaches this. */
	void ServerBroadcastSystem(const FString& Body);

	/** COMMS-1 STUB -- always false. COMMS-5 wires block persistence behind this exact signature (the seam
	 *  ships now so the fan-out already consults it). */
	static bool IsBlockedBy(const APlayerController* Recipient, const APlayerController* Sender);

	/** Strip control characters and hard-clamp to AFLChat::MaxBodyLength. Static + public so the harness/tests
	 *  can assert the exact server behavior. */
	static FString SanitizeBody(const FString& In);

protected:
	virtual void BeginPlay() override;

	/** The authoritative pipeline. WithValidation: a client that sends the System channel (server-only) or a
	 *  malformed channel fails validation -> disconnect-grade, per Lyra convention. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSendChat(const FAFLChatMessage& Message);

	/** Owner-only delivery of one message to THIS client. */
	UFUNCTION(Client, Reliable)
	void ClientReceiveChat(const FAFLChatMessage& Message);

	/** Owner-only: tell the SENDER their outbound was dropped (rate / filter / Party-reserved / invalid). The
	 *  client handler synthesizes a LOCAL dim System line (bLocalEphemeral) and broadcasts it straight to the
	 *  UI subscription -- never OnInbound, never the 200-ring. COMMS-2 drop-echo (Ruling 1): the sender learns
	 *  they were throttled instead of concluding chat is broken. */
	UFUNCTION(Client, Reliable)
	void ClientReceiveChatDropped(EAFLChatDropReason Reason);

private:
	// --- server-side per-player token bucket (burst 5, refill 1/s) ---
	static constexpr float RateBurst = 5.0f;
	static constexpr float RateRefillPerSec = 1.0f;
	float RateTokens = RateBurst;
	double LastRefillSeconds = -1.0;

	/** Server-side content-filter seam (passthrough in COMMS-1). */
	TSharedPtr<IAFLChatFilter> ServerFilter;

	/** Validate the inbound request, stamp server-authoritative identity / ServerTimestamp / ServerEpochMs,
	 *  force-clear the client-only bLocalEphemeral flag, and clamp the body. Returns false (with a logged DROP
	 *  reason and OutReason set) if the message must not proceed. Server only. */
	bool ServerAcceptAndStamp(FAFLChatMessage& Msg, APlayerController*& OutSenderPC, APlayerState*& OutSenderPS,
		EAFLChatDropReason& OutReason);

	/** Refill + try to consume one rate token. False -> rate-limited. Server only. */
	bool ConsumeRateToken();

	/** Resolve recipients for the channel and deliver via each recipient component's ClientReceiveChat. */
	void ServerFanOut(const FAFLChatMessage& Msg, APlayerController* SenderPC, APlayerState* SenderPS);

	/** Find a PC's chat component and fire its owner-only ClientReceiveChat (honors IsBlockedBy). */
	void DeliverToRecipient(APlayerController* RecipientPC, APlayerController* SenderPC, const FAFLChatMessage& Msg);
};

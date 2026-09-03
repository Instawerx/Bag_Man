// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AFLChatTypes.h"

/**
 * IAFLChatTransport -- the SEND/RECEIVE transport SEAM. COMMS-1 ships exactly ONE implementation
 * (ServerReplicated, via UAFLChatComponent's owner-only RPCs); EOSP2P (COMMS-3) and AWSWebSocket (COMMS-5)
 * are future implementations that plug in behind this same interface, so the subsystem never learns how a
 * message physically travels.
 *
 * Plain C++ (non-UObject) -- an engine-side seam owned by the subsystem, keyed by IsAvailable(). The
 * subsystem picks the first available transport and routes Send through it; inbound arrives on
 * OnMessageReceived regardless of the transport's physical mechanism.
 */
class IAFLChatTransport
{
public:
	virtual ~IAFLChatTransport() = default;

	/** A short stable name for logs / the transport registry (e.g. "ServerReplicated"). */
	virtual FName GetTransportName() const = 0;

	/** True when this transport can currently carry a message (e.g. the local chat component exists and has a
	 *  net connection). The subsystem routes Send to the first available transport. */
	virtual bool IsAvailable() const = 0;

	/** Hand an OUTBOUND message to the transport. The transport is responsible ONLY for delivery; the
	 *  subsystem has already run the outbound filter. Server-side validation/fan-out belongs to the receiving
	 *  end (the server), never the transport. */
	virtual void SendMessage(const FAFLChatMessage& Message) = 0;

	/** Fired when a message arrives for THIS client through this transport. The subsystem subscribes to the
	 *  active transport's delegate; the transport is what actually invokes it (the ServerReplicated transport
	 *  fires it from the component's owner-only ClientReceiveChat). */
	DECLARE_MULTICAST_DELEGATE_OneParam(FAFLOnChatMessageReceived, const FAFLChatMessage& /*Message*/);
	FAFLOnChatMessageReceived OnMessageReceived;
};

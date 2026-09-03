// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FAFLChatMessage;

/**
 * IAFLChatFilter -- the profanity / content filter SEAM (COMMS-1 ships the passthrough; a real service is a
 * later phase). Two hooks so outbound (what this client sends) and inbound (what this client displays) can
 * be filtered independently -- a client may want to see raw text it sent while still masking inbound.
 *
 * Plain C++ (non-UObject) -- it is an internal engine-side seam, not a Blueprint surface. Returns the
 * (possibly transformed) body; returning false from ShouldBlock drops the message entirely.
 */
class IAFLChatFilter
{
public:
	virtual ~IAFLChatFilter() = default;

	/** Transform/inspect an OUTBOUND body before it leaves this client. Return the body to send (may be
	 *  unchanged). Set bOutBlock=true to suppress the send entirely. */
	virtual FString FilterOutbound(const FString& Body, bool& bOutBlock) const = 0;

	/** Transform/inspect an INBOUND body before this client displays it. Return the body to display.
	 *  Set bOutBlock=true to hide the message. */
	virtual FString FilterInbound(const FString& Body, bool& bOutBlock) const = 0;
};

/**
 * FAFLPassthroughChatFilter -- the COMMS-1 default: no transformation, nothing blocked. The seam ships now;
 * the real profanity service swaps in behind this same interface in a later phase without touching the
 * subsystem or the transport.
 */
class FAFLPassthroughChatFilter final : public IAFLChatFilter
{
public:
	virtual FString FilterOutbound(const FString& Body, bool& bOutBlock) const override { bOutBlock = false; return Body; }
	virtual FString FilterInbound(const FString& Body, bool& bOutBlock) const override { bOutBlock = false; return Body; }
};

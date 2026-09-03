// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h" // FUniqueNetIdRepl (module: Engine)

#include "AFLChatTypes.generated.h"

/**
 * EAFLChatChannel -- the text-chat channels (COMMS-1 spine).
 *
 * Party is RESERVED for COMMS-3 (EOS party rooms) and is REJECTED by the server pipeline in COMMS-1 --
 * it exists in the enum so the wire format and the switch statements are stable across the phase boundary.
 * System is server-originated ONLY; a client that sends System is a validation failure (disconnect-grade,
 * per the Lyra ServerRPC-WithValidation convention).
 */
UENUM(BlueprintType)
enum class EAFLChatChannel : uint8
{
	Say		UMETA(DisplayName = "Say"),		// everyone connected
	Team	UMETA(DisplayName = "Team"),	// same team as sender (ULyraTeamSubsystem / FGenericTeamId)
	Party	UMETA(DisplayName = "Party"),	// RESERVED -- COMMS-3; rejected server-side in COMMS-1
	Whisper	UMETA(DisplayName = "Whisper"),	// one target + echo to sender
	System	UMETA(DisplayName = "System"),	// server-originated only

	MAX		UMETA(Hidden)
};

/** Server-enforced hard cap on a chat body, in characters. Also declared here so the client CAN pre-trim
 *  for UX, but the SERVER is the only authority -- it re-clamps every inbound message (COMMS-2+ must not
 *  assume the client trimmed). */
namespace AFLChat
{
	static constexpr int32 MaxBodyLength = 256;
}

/**
 * FAFLChatMessage -- one replicated chat message (COMMS-1).
 *
 * PLAIN-PROPERTY replication (no custom NetSerialize): it travels as a Server/Client RPC parameter, so UE
 * serializes each UPROPERTY field-by-field and FUniqueNetIdRepl brings its own net-aware serializer. If a
 * custom NetSerializer is ever forced (payload tightening), it MUST stay in this always-loaded AFLNetTypes
 * module per the FNetSerializeScriptStructCache load-order law (see AFLAbilityTargetData_Hitscan.h) -- never
 * move it into a GameFeature.
 *
 * TRUST BOUNDARY: SenderId, SenderDisplayName and ServerTimestamp are SERVER-STAMPED. The server ignores
 * whatever a client puts in them on ServerSendChat and overwrites them from the authoritative PlayerState /
 * connection before fan-out, so a client can never spoof another player's name or identity. On the outbound
 * ServerSendChat the client meaningfully fills only Channel, Body and (for Whisper) TargetId.
 */
USTRUCT(BlueprintType)
struct AFLNETTYPES_API FAFLChatMessage
{
	GENERATED_BODY()

	/** Which channel this message is on. */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Chat")
	EAFLChatChannel Channel = EAFLChatChannel::Say;

	/** The sender's unique net id. SERVER-STAMPED (from the sending connection's PlayerState); never client-trusted. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Chat")
	FUniqueNetIdRepl SenderId;

	/** The sender's display name for UI. SERVER-STAMPED (PlayerState GetPlayerName); never client-trusted. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Chat")
	FString SenderDisplayName;

	/** Whisper target's unique net id. Meaningful ONLY on the Whisper channel; empty otherwise. */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Chat")
	FUniqueNetIdRepl TargetId;

	/** The message text. Hard cap AFLChat::MaxBodyLength (256), SERVER-ENFORCED (re-clamped + control-chars stripped). */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Chat")
	FString Body;

	/** Seconds since the server's app start (FPlatformTime / world time) at fan-out. SERVER-STAMPED. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Chat")
	double ServerTimestamp = 0.0;

	FAFLChatMessage() = default;
};

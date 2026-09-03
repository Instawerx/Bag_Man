// Copyright C12 AI Gaming. All Rights Reserved.

#include "Chat/AFLChatComponent.h"

#include "Chat/AFLChatFilter.h"
#include "Chat/AFLChatSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GenericTeamAgentInterface.h" // IGenericTeamAgentInterface (module: AIModule) -- team resolution
#include "HAL/PlatformTime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLChatComponent)

DEFINE_LOG_CATEGORY(LogAFLChat);

namespace
{
	// Log-safe channel name.
	const TCHAR* ChannelName(EAFLChatChannel C)
	{
		switch (C)
		{
		case EAFLChatChannel::Say:     return TEXT("Say");
		case EAFLChatChannel::Team:    return TEXT("Team");
		case EAFLChatChannel::Party:   return TEXT("Party");
		case EAFLChatChannel::Whisper: return TEXT("Whisper");
		case EAFLChatChannel::System:  return TEXT("System");
		default:                       return TEXT("Invalid");
		}
	}

	// A short, non-PII sender tag for logs (never the display name, never a Whisper body).
	FString SenderTag(const APlayerState* PS)
	{
		return PS ? FString::Printf(TEXT("pid=%d"), PS->GetPlayerId()) : TEXT("pid=?");
	}
}

UAFLChatComponent::UAFLChatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true); // owner-only RPCs both directions; no replicated properties in COMMS-1
	ServerFilter = MakeShared<FAFLPassthroughChatFilter>();
}

void UAFLChatComponent::BeginPlay()
{
	Super::BeginPlay();

	// The LOCAL player's component registers with the subsystem so inbound is caught immediately (before the
	// player ever sends). A dedicated server has no local controller -> no registration (no local UI there).
	if (const APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		if (PC->IsLocalController())
		{
			if (UAFLChatSubsystem* Subsys = UAFLChatSubsystem::Get(this))
			{
				Subsys->RegisterLocalComponent(this);
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
// CLIENT -> SERVER submit
// ------------------------------------------------------------------------------------------------

void UAFLChatComponent::SubmitOutbound(const FAFLChatMessage& Message)
{
	// Owning client only -- the RPC is Server-routed, so a non-owning caller would be dropped anyway; guard
	// so a mistaken server/proxy call is a no-op rather than a warning storm.
	if (const AActor* Owner = GetOwner())
	{
		if (Owner->GetLocalRole() == ROLE_AutonomousProxy || Owner->GetLocalRole() == ROLE_Authority)
		{
			ServerSendChat(Message);
		}
	}
}

bool UAFLChatComponent::ServerSendChat_Validate(const FAFLChatMessage& Message)
{
	// DISCONNECT-GRADE rejects (only for things a legitimate client can never send):
	//  - System channel: server-originated only.
	//  - out-of-range channel enum.
	// Everything else (rate, length, Party-reserved) is a soft server-side DROP, not a validation failure.
	if (Message.Channel == EAFLChatChannel::System) { return false; }
	if (static_cast<uint8>(Message.Channel) >= static_cast<uint8>(EAFLChatChannel::MAX)) { return false; }
	return true;
}

// ------------------------------------------------------------------------------------------------
// SERVER pipeline
// ------------------------------------------------------------------------------------------------

FString UAFLChatComponent::SanitizeBody(const FString& In)
{
	FString Out;
	Out.Reserve(FMath::Min(In.Len(), AFLChat::MaxBodyLength));
	for (const TCHAR Ch : In)
	{
		// Strip C0 control chars (incl newlines/tabs) and DEL; keep everything printable.
		if (Ch >= 0x20 && Ch != 0x7F)
		{
			Out.AppendChar(Ch);
			if (Out.Len() >= AFLChat::MaxBodyLength) { break; } // hard clamp
		}
	}
	Out.TrimStartAndEndInline();
	return Out;
}

bool UAFLChatComponent::ConsumeRateToken()
{
	const double Now = FPlatformTime::Seconds();
	if (LastRefillSeconds < 0.0) { LastRefillSeconds = Now; }
	const double Elapsed = Now - LastRefillSeconds;
	if (Elapsed > 0.0)
	{
		RateTokens = FMath::Min(RateBurst, RateTokens + static_cast<float>(Elapsed) * RateRefillPerSec);
		LastRefillSeconds = Now;
	}
	if (RateTokens >= 1.0f)
	{
		RateTokens -= 1.0f;
		return true;
	}
	return false;
}

bool UAFLChatComponent::ServerAcceptAndStamp(FAFLChatMessage& Msg, APlayerController*& OutSenderPC, APlayerState*& OutSenderPS)
{
	OutSenderPC = Cast<APlayerController>(GetOwner());
	OutSenderPS = OutSenderPC ? OutSenderPC->PlayerState : nullptr;

	// auth-complete check: a real, connected, non-bot player.
	if (!OutSenderPC || !OutSenderPS || OutSenderPS->IsABot())
	{
		UE_LOG(LogAFLChat, Warning, TEXT("AFL_CHAT[DROP_INVALID] no valid sender PlayerState (%s)"), *SenderTag(OutSenderPS));
		return false;
	}

	// Party is reserved for COMMS-3 -- reject with a clear reason (soft drop, not a disconnect).
	if (Msg.Channel == EAFLChatChannel::Party)
	{
		UE_LOG(LogAFLChat, Log, TEXT("AFL_CHAT[DROP_INVALID] %s ch=Party reason=reserved(COMMS-3)"), *SenderTag(OutSenderPS));
		return false;
	}

	// System from a client should already be gone (Validate), but fail closed if it slips through.
	if (Msg.Channel == EAFLChatChannel::System)
	{
		UE_LOG(LogAFLChat, Warning, TEXT("AFL_CHAT[DROP_INVALID] %s ch=System reason=server-only"), *SenderTag(OutSenderPS));
		return false;
	}

	// Body: strip control chars + hard clamp. Empty after sanitize -> drop.
	Msg.Body = SanitizeBody(Msg.Body);
	if (Msg.Body.IsEmpty())
	{
		UE_LOG(LogAFLChat, Log, TEXT("AFL_CHAT[DROP_INVALID] %s ch=%s reason=empty-after-sanitize"),
			*SenderTag(OutSenderPS), ChannelName(Msg.Channel));
		return false;
	}

	// Server-side content filter seam (passthrough in COMMS-1).
	bool bFilterBlock = false;
	if (ServerFilter.IsValid())
	{
		Msg.Body = ServerFilter->FilterOutbound(Msg.Body, bFilterBlock);
	}
	if (bFilterBlock || Msg.Body.IsEmpty())
	{
		UE_LOG(LogAFLChat, Log, TEXT("AFL_CHAT[DROP_FILTER] %s ch=%s"), *SenderTag(OutSenderPS), ChannelName(Msg.Channel));
		return false;
	}

	// SERVER-STAMP authoritative identity + time (overwrite anything the client put here).
	Msg.SenderId = OutSenderPS->GetUniqueId();
	Msg.SenderDisplayName = OutSenderPS->GetPlayerName();
	Msg.ServerTimestamp = FPlatformTime::Seconds();
	return true;
}

void UAFLChatComponent::ServerSendChat_Implementation(const FAFLChatMessage& Message)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return; // authority guard -- pipeline is server only
	}

	FAFLChatMessage Msg = Message;
	APlayerController* SenderPC = nullptr;
	APlayerState* SenderPS = nullptr;

	if (!ServerAcceptAndStamp(Msg, SenderPC, SenderPS))
	{
		return; // reason already logged
	}

	if (!ConsumeRateToken())
	{
		UE_LOG(LogAFLChat, Log, TEXT("AFL_CHAT[DROP_RATE] %s ch=%s len=%d"),
			*SenderTag(SenderPS), ChannelName(Msg.Channel), Msg.Body.Len());
		return;
	}

	// SEND accepted. Whisper body is NEVER logged (private); Say/Team ARE (moderation needs them).
	if (Msg.Channel == EAFLChatChannel::Whisper)
	{
		UE_LOG(LogAFLChat, Log, TEXT("AFL_CHAT[SEND] %s ch=Whisper len=%d (body withheld)"), *SenderTag(SenderPS), Msg.Body.Len());
	}
	else
	{
		UE_LOG(LogAFLChat, Log, TEXT("AFL_CHAT[SEND] %s ch=%s len=%d body='%s'"),
			*SenderTag(SenderPS), ChannelName(Msg.Channel), Msg.Body.Len(), *Msg.Body);
	}

	ServerFanOut(Msg, SenderPC, SenderPS);
}

void UAFLChatComponent::ServerFanOut(const FAFLChatMessage& Msg, APlayerController* SenderPC, APlayerState* SenderPS)
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	switch (Msg.Channel)
	{
	case EAFLChatChannel::Say:
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->PlayerState && !PC->PlayerState->IsABot())
			{
				DeliverToRecipient(PC, SenderPC, Msg);
			}
		}
		break;
	}
	case EAFLChatChannel::Team:
	{
		FGenericTeamId SenderTeam = FGenericTeamId::NoTeam;
		if (const IGenericTeamAgentInterface* SenderAgent = Cast<IGenericTeamAgentInterface>(SenderPS))
		{
			SenderTeam = SenderAgent->GetGenericTeamId();
		}
		if (SenderTeam == FGenericTeamId::NoTeam) { break; } // no team -> nobody to reach
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			APlayerState* PS = PC ? PC->PlayerState : nullptr;
			if (!PS || PS->IsABot()) { continue; }
			const IGenericTeamAgentInterface* Agent = Cast<IGenericTeamAgentInterface>(PS);
			if (Agent && Agent->GetGenericTeamId() == SenderTeam)
			{
				DeliverToRecipient(PC, SenderPC, Msg);
			}
		}
		break;
	}
	case EAFLChatChannel::Whisper:
	{
		APlayerController* TargetPC = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->PlayerState && PC->PlayerState->GetUniqueId() == Msg.TargetId)
			{
				TargetPC = PC;
				break;
			}
		}
		if (TargetPC)
		{
			DeliverToRecipient(TargetPC, SenderPC, Msg);
		}
		else
		{
			UE_LOG(LogAFLChat, Log, TEXT("AFL_CHAT[DROP_INVALID] %s ch=Whisper reason=target-not-found"), *SenderTag(SenderPS));
		}
		// Echo to the sender so they see their own whisper (never blocked by IsBlockedBy).
		if (SenderPC)
		{
			if (UAFLChatComponent* SelfComp = SenderPC->FindComponentByClass<UAFLChatComponent>())
			{
				SelfComp->ClientReceiveChat(Msg);
			}
		}
		break;
	}
	case EAFLChatChannel::System:
	case EAFLChatChannel::Party:
	case EAFLChatChannel::MAX:
	default:
		break; // unreachable for client sends (rejected earlier)
	}
}

void UAFLChatComponent::DeliverToRecipient(APlayerController* RecipientPC, APlayerController* SenderPC, const FAFLChatMessage& Msg)
{
	if (!RecipientPC) { return; }
	if (IsBlockedBy(RecipientPC, SenderPC)) { return; } // COMMS-5 seam; false in COMMS-1
	if (UAFLChatComponent* Comp = RecipientPC->FindComponentByClass<UAFLChatComponent>())
	{
		Comp->ClientReceiveChat(Msg); // owner-only client RPC -> reaches exactly that connection
	}
}

void UAFLChatComponent::ServerBroadcastSystem(const FString& Body)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	UWorld* World = GetWorld();
	if (!World) { return; }

	FAFLChatMessage Msg;
	Msg.Channel = EAFLChatChannel::System;
	Msg.Body = SanitizeBody(Body);
	Msg.SenderDisplayName = TEXT("SYSTEM");
	Msg.ServerTimestamp = FPlatformTime::Seconds();
	if (Msg.Body.IsEmpty()) { return; }

	UE_LOG(LogAFLChat, Log, TEXT("AFL_CHAT[SEND] SYSTEM ch=System len=%d body='%s'"), Msg.Body.Len(), *Msg.Body);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->PlayerState && !PC->PlayerState->IsABot())
		{
			DeliverToRecipient(PC, /*SenderPC*/ nullptr, Msg);
		}
	}
}

// ------------------------------------------------------------------------------------------------
// SERVER -> CLIENT delivery
// ------------------------------------------------------------------------------------------------

void UAFLChatComponent::ClientReceiveChat_Implementation(const FAFLChatMessage& Message)
{
	// Runs on the owning client only (Client RPC). Hand to whoever bound OnInbound (the transport -> subsystem).
	OnInbound.Broadcast(Message);
}

// ------------------------------------------------------------------------------------------------
// Block seam (COMMS-5)
// ------------------------------------------------------------------------------------------------

bool UAFLChatComponent::IsBlockedBy(const APlayerController* /*Recipient*/, const APlayerController* /*Sender*/)
{
	return false; // COMMS-1: nobody blocks anybody yet. COMMS-5 wires persistence here.
}

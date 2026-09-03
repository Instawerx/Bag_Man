// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/TimerHandle.h"       // FTimerHandle (reconnect backoff)
#include "Templates/Function.h"

#include "AFLSocialSubsystem.generated.h"

class IWebSocket;

/**
 * FAFLDirectMessage -- one direct message (COMMS-5 DM). NOT net-serialized: it is backend JSON over a
 * WebSocket / REST, never a UE replication RPC, so it lives in AFLOnline (not AFLNetTypes). Keys on the
 * PlayFab player id (the DM backend's id space), NOT the FUniqueNetIdRepl the in-match text spine uses.
 */
USTRUCT(BlueprintType)
struct AFLONLINE_API FAFLDirectMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AFL|DM") FString ConversationId;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|DM") FString MsgId;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|DM") FString SenderId;     // PlayFabId
	UPROPERTY(BlueprintReadOnly, Category = "AFL|DM") FString RecipientId;  // PlayFabId
	UPROPERTY(BlueprintReadOnly, Category = "AFL|DM") FString Body;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|DM") int64   ServerEpochMs = 0; // serverTs (epoch ms, wall-clock)
	UPROPERTY(BlueprintReadOnly, Category = "AFL|DM") bool    bRead = false;     // readAt != null

	FAFLDirectMessage() = default;
};

/**
 * UAFLSocialSubsystem -- COMMS-5 game-side DM client (the first slice of Social; friends / presence / block
 * persistence land in later increments). It connects the persistent DM WebSocket after PlayFab login
 * (session-ticket auth, browser-style ?ticket= since a UE WebSocket sets no custom headers reliably at
 * connect), delivers inbound DMs to the UI via OnDirectMessage, sends via SendDirectMessage, and backfills the
 * offline inbox + marks-read over REST. Everything keys on PlayFabId.
 *
 * Client-only by construction: it connects only on a login-bearing, non-dedicated-server process. Transport
 * URLs are CONFIG (DefaultGame.ini [AFL.Online] DirectMessageWebSocketUrl; the REST base reuses
 * UAFLOnlineSubsystem::PlayerApiBaseUrl) -- never secrets. The tabbed Session/Inbox UI (R5) consumes this;
 * COMMS-2's DM tab stays a disabled stub until it lands.
 */
UCLASS()
class AFLONLINE_API UAFLSocialSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UAFLSocialSubsystem* Get(const UObject* WorldContext);

	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Send a DM to a recipient (their PlayFabId) over the live WebSocket. No-op (logged) if not connected. */
	void SendDirectMessage(const FString& RecipientPlayFabId, const FString& Body);

	/** Backfill the offline inbox since a serverTs cursor (0 = all). Callback gets the messages + next cursor. */
	void FetchInbox(int64 SinceEpochMs, TFunction<void(const TArray<FAFLDirectMessage>&, int64 /*cursor*/)> OnComplete);

	/** Mark a conversation read: POST /messages/{conversationId}/read. */
	void MarkConversationRead(const FString& ConversationId, TFunction<void(bool)> OnComplete = TFunction<void(bool)>());

	/** Fired on the local client for each delivered DM (live WebSocket). The UI subscribes; release on teardown. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FAFLOnDirectMessage, const FAFLDirectMessage& /*Message*/);
	FAFLOnDirectMessage OnDirectMessage;

	/** Fired when the DM socket connects/disconnects (for a presence dot / retry UX). */
	DECLARE_MULTICAST_DELEGATE_OneParam(FAFLOnDMConnectionChanged, bool /*bConnected*/);
	FAFLOnDMConnectionChanged OnConnectionChanged;

	bool IsConnected() const { return bConnected; }

private:
	void ConnectWebSocket();
	void CloseWebSocket();
	void HandleWsConnected();
	void HandleWsConnectionError(const FString& Error);
	void HandleWsClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void HandleWsMessage(const FString& MessageJson);
	void ScheduleReconnect();

	/** [AFLOnline] DirectMessageWebSocketUrl from DefaultGame.ini (env AFL_DM_WS_URL wins for server tooling). */
	FString DmWebSocketUrl() const;

	TSharedPtr<IWebSocket> Socket;
	FDelegateHandle LoggedInHandle;
	bool bConnected = false;
	bool bWantConnected = false;      // we intend to be connected -> reconnect on an unclean close
	int32 ReconnectAttempts = 0;
	FTimerHandle ReconnectTimer;

	static constexpr int32 MaxReconnectAttempts = 6;
};

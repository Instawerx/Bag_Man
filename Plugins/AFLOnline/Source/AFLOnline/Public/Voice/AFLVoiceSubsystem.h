// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "AFLVoiceSubsystem.generated.h"

class IVoiceChat;

/**
 * UAFLVoiceSubsystem -- COMMS-3/4 game-side voice CLIENT. Wraps the engine IVoiceChat modular feature (feature
 * name "VoiceChat"), which the EOSVoiceChat plugin backs with EOS RTC. Client-only. IVoiceChat IS-A
 * IVoiceChatUser, so the single local user is driven directly on the IVoiceChat*.
 *
 * Verified lifecycle (UE 5.6 VoiceChat.h): Initialize -> Connect -> Login(FPlatformUserId, PlayerName, Creds)
 * -> JoinChannel(name, credentials, type). For EOSVoiceChat specifically: PlayerName = the local EOS
 * ProductUserId (puid) STRING and Creds is IGNORED; JoinChannel credentials is a JSON string
 * {"participant_token","client_base_url","override_userid"} minted per puid+room by the COMMS-4B /rtc/token
 * backend. Every async op reports success only via its FVoiceChatResult completion delegate.
 *
 * THIS PASS = the client core: lifecycle + JoinRoom(token) + mute/PTT/per-player volume + a talking delegate,
 * driven by afl.Voice.* console commands (dev args) for a 2-client audio PIE. NEXT = the server-owned-room
 * auto-flow (dedicated server mints via PostServerRtcToken + distributes per-puid tokens + R1 proximity) and
 * the party-lobby path. GATE: the [EOSVoiceChat] EOS config (ProductId..ClientSecret) in the EOS overlay
 * (operator-placed secret) + a 2-client audio PIE; inert/no-op until a voice provider + login exist.
 */
UCLASS()
class AFLONLINE_API UAFLVoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UAFLVoiceSubsystem* Get(const UObject* WorldContext);

	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Bring voice up: Initialize -> Connect -> Login as this puid (EOS ProductUserId string). Async; idempotent. */
	void StartVoice(const FString& LocalProductUserId);

	/** Join an EOS RTC room with a minted participant token (from /rtc/token). bPositional -> proximity channel. */
	void JoinRoom(const FString& RoomName, const FString& ParticipantToken, const FString& ClientBaseUrl, bool bPositional);

	/** Dev best-effort mic loopback (Echo channel via InsecureGetJoinToken). EOS RTC may reject insecure tokens. */
	void JoinEchoTest(const FString& ChannelName);

	void LeaveRoom(const FString& RoomName);

	/** Local mic device mute. */
	void SetInputMuted(bool bMuted);

	/** Open-mic vs push-to-talk: true = transmit to all joined channels, false = transmit to none. */
	void SetTransmitting(bool bTransmitting);

	/** Per-player output volume (0.0-2.0, 1.0 = unchanged). The v1 proximity-attenuation knob. */
	void SetPeerVolume(const FString& PlayerName, float Volume);

	/** True when a voice provider exists, is initialized/connected, and the local user is logged in. */
	bool IsVoiceReady() const;

	/** Fired when a peer starts/stops talking (drives the faceplate speaking pulse). */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FAFLOnPeerTalking, const FString& /*Channel*/, const FString& /*Player*/, bool /*bTalking*/);
	FAFLOnPeerTalking OnPeerTalking;

private:
	IVoiceChat* GetVoice() const;
	void ConnectThenLogin();
	void DoLogin();
	void BindDelegates();
	void UnbindDelegates();

	FString LoginPuid;
	bool bDelegatesBound = false;
	FDelegateHandle TalkingHandle;
};

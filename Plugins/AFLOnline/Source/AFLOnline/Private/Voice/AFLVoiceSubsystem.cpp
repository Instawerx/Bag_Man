// Copyright C12 AI Gaming. All Rights Reserved.

#include "Voice/AFLVoiceSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// VoiceChat is a CLIENT-ONLY module; on the dedicated server it does not exist, so all IVoiceChat code is
// compiled only when AFLONLINE_WITH_VOICE (set per-target in AFLOnline.Build.cs). The UCLASS header stays
// unconditional so UHT + the subsystem registration are identical on every target; the methods are no-ops
// on the server.
#if AFLONLINE_WITH_VOICE
#include "VoiceChat.h"          // IVoiceChat / IVoiceChatUser + delegates + EVoiceChatChannelType
#include "VoiceChatResult.h"    // FVoiceChatResult
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLVoiceSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogAFLVoice, Log, All);

UAFLVoiceSubsystem* UAFLVoiceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UAFLVoiceSubsystem>() : nullptr;
}

IVoiceChat* UAFLVoiceSubsystem::GetVoice() const
{
#if AFLONLINE_WITH_VOICE
	return IVoiceChat::Get(); // modular feature "VoiceChat"; null unless a provider (EOSVoiceChat) loaded
#else
	return nullptr;
#endif
}

void UAFLVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IsRunningDedicatedServer())
	{
		return; // voice is a client concern; the server mints tokens (PostServerRtcToken), it never joins rooms
	}
	BindDelegates();
}

void UAFLVoiceSubsystem::Deinitialize()
{
	UnbindDelegates();
	Super::Deinitialize();
}

void UAFLVoiceSubsystem::BindDelegates()
{
#if AFLONLINE_WITH_VOICE
	if (bDelegatesBound) { return; }
	IVoiceChat* VC = GetVoice();
	if (!VC) { return; } // provider not loaded yet -- bound lazily on the next StartVoice
	TalkingHandle = VC->OnVoiceChatPlayerTalkingUpdated().AddWeakLambda(this,
		[this](const FString& Channel, const FString& Player, bool bTalking)
		{
			OnPeerTalking.Broadcast(Channel, Player, bTalking);
		});
	bDelegatesBound = true;
#endif
}

void UAFLVoiceSubsystem::UnbindDelegates()
{
#if AFLONLINE_WITH_VOICE
	if (!bDelegatesBound) { return; }
	if (IVoiceChat* VC = GetVoice())
	{
		VC->OnVoiceChatPlayerTalkingUpdated().Remove(TalkingHandle);
	}
	TalkingHandle.Reset();
	bDelegatesBound = false;
#endif
}

void UAFLVoiceSubsystem::StartVoice(const FString& LocalProductUserId)
{
#if AFLONLINE_WITH_VOICE
	IVoiceChat* VC = GetVoice();
	if (!VC)
	{
		UE_LOG(LogAFLVoice, Warning, TEXT("AFL_VOICE: no voice provider (enable EOSVoiceChat + [EOSVoiceChat] config)."));
		return;
	}
	if (LocalProductUserId.IsEmpty())
	{
		UE_LOG(LogAFLVoice, Warning, TEXT("AFL_VOICE: StartVoice with empty puid -- login needs the local EOS ProductUserId."));
		return;
	}
	LoginPuid = LocalProductUserId;
	BindDelegates(); // provider may have loaded since Initialize

	if (!VC->IsInitialized())
	{
		VC->Initialize(FOnVoiceChatInitializeCompleteDelegate::CreateWeakLambda(this, [this](const FVoiceChatResult& Result)
		{
			if (!Result.IsSuccess())
			{
				UE_LOG(LogAFLVoice, Warning, TEXT("AFL_VOICE: Initialize failed: %s"), *Result.ErrorDesc);
				return;
			}
			ConnectThenLogin();
		}));
	}
	else
	{
		ConnectThenLogin();
	}
#endif
}

void UAFLVoiceSubsystem::ConnectThenLogin()
{
#if AFLONLINE_WITH_VOICE
	IVoiceChat* VC = GetVoice();
	if (!VC) { return; }
	if (!VC->IsConnected())
	{
		VC->Connect(FOnVoiceChatConnectCompleteDelegate::CreateWeakLambda(this, [this](const FVoiceChatResult& Result)
		{
			if (!Result.IsSuccess())
			{
				UE_LOG(LogAFLVoice, Warning, TEXT("AFL_VOICE: Connect failed: %s"), *Result.ErrorDesc);
				return;
			}
			DoLogin();
		}));
	}
	else
	{
		DoLogin();
	}
#endif
}

void UAFLVoiceSubsystem::DoLogin()
{
#if AFLONLINE_WITH_VOICE
	IVoiceChat* VC = GetVoice();
	if (!VC || LoginPuid.IsEmpty()) { return; }
	if (VC->IsLoggedIn())
	{
		UE_LOG(LogAFLVoice, Log, TEXT("AFL_VOICE: already logged in as %s"), *VC->GetLoggedInPlayerName());
		return;
	}
	// FPlatformUserId is largely informational for EOSVoiceChat (it keys on PlayerName=puid); use the local player's.
	FPlatformUserId PlatformUser = PLATFORMUSERID_NONE;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const ULocalPlayer* LP = GI->GetLocalPlayerByIndex(0))
		{
			PlatformUser = LP->GetPlatformUserId();
		}
	}
	VC->Login(PlatformUser, LoginPuid, /*Credentials (ignored by EOSVoiceChat)*/ FString(),
		FOnVoiceChatLoginCompleteDelegate::CreateWeakLambda(this, [this](const FString& PlayerName, const FVoiceChatResult& Result)
		{
			UE_LOG(LogAFLVoice, Log, TEXT("AFL_VOICE: Login %s -> %s"),
				*PlayerName, Result.IsSuccess() ? TEXT("OK") : *Result.ErrorDesc);
		}));
#endif
}

void UAFLVoiceSubsystem::JoinRoom(const FString& RoomName, const FString& ParticipantToken, const FString& ClientBaseUrl, bool bPositional)
{
#if AFLONLINE_WITH_VOICE
	IVoiceChat* VC = GetVoice();
	if (!VC || !VC->IsLoggedIn())
	{
		UE_LOG(LogAFLVoice, Warning, TEXT("AFL_VOICE: JoinRoom '%s' before login -- call StartVoice first."), *RoomName);
		return;
	}
	// EOS channel credentials are a JSON string carrying the RTC participant token + client base URL.
	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("participant_token"), ParticipantToken);
	Obj->SetStringField(TEXT("client_base_url"), ClientBaseUrl);
	Obj->SetStringField(TEXT("override_userid"), TEXT(""));
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Obj, Writer);

	const EVoiceChatChannelType Type = bPositional ? EVoiceChatChannelType::Positional : EVoiceChatChannelType::NonPositional;
	VC->JoinChannel(RoomName, Json, Type,
		FOnVoiceChatChannelJoinCompleteDelegate::CreateWeakLambda(this, [this](const FString& Channel, const FVoiceChatResult& Result)
		{
			UE_LOG(LogAFLVoice, Log, TEXT("AFL_VOICE: JoinChannel '%s' -> %s"),
				*Channel, Result.IsSuccess() ? TEXT("OK") : *Result.ErrorDesc);
			if (Result.IsSuccess())
			{
				if (IVoiceChat* V = GetVoice()) { V->TransmitToAllChannels(); } // open-mic default; PTT toggles
			}
		}));
#endif
}

void UAFLVoiceSubsystem::JoinEchoTest(const FString& ChannelName)
{
#if AFLONLINE_WITH_VOICE
	IVoiceChat* VC = GetVoice();
	if (!VC || !VC->IsLoggedIn())
	{
		UE_LOG(LogAFLVoice, Warning, TEXT("AFL_VOICE: JoinEchoTest before login -- call StartVoice first."));
		return;
	}
	// Dev-only: InsecureGetJoinToken. EOS RTC may reject insecure tokens -- if so, use JoinRoom with a real mint.
	const FString Token = VC->InsecureGetJoinToken(ChannelName, EVoiceChatChannelType::Echo);
	VC->JoinChannel(ChannelName, Token, EVoiceChatChannelType::Echo,
		FOnVoiceChatChannelJoinCompleteDelegate::CreateWeakLambda(this, [this](const FString& Channel, const FVoiceChatResult& Result)
		{
			UE_LOG(LogAFLVoice, Log, TEXT("AFL_VOICE: Echo '%s' -> %s"),
				*Channel, Result.IsSuccess() ? TEXT("OK") : *Result.ErrorDesc);
			if (Result.IsSuccess())
			{
				if (IVoiceChat* V = GetVoice()) { V->TransmitToAllChannels(); }
			}
		}));
#endif
}

void UAFLVoiceSubsystem::LeaveRoom(const FString& RoomName)
{
#if AFLONLINE_WITH_VOICE
	if (IVoiceChat* VC = GetVoice())
	{
		VC->LeaveChannel(RoomName, FOnVoiceChatChannelLeaveCompleteDelegate::CreateWeakLambda(this, [](const FString& Channel, const FVoiceChatResult& Result)
		{
			UE_LOG(LogAFLVoice, Log, TEXT("AFL_VOICE: LeaveChannel '%s' -> %s"), *Channel, Result.IsSuccess() ? TEXT("OK") : *Result.ErrorDesc);
		}));
	}
#endif
}

void UAFLVoiceSubsystem::SetInputMuted(bool bMuted)
{
#if AFLONLINE_WITH_VOICE
	if (IVoiceChat* VC = GetVoice()) { VC->SetAudioInputDeviceMuted(bMuted); }
#endif
}

void UAFLVoiceSubsystem::SetTransmitting(bool bTransmitting)
{
#if AFLONLINE_WITH_VOICE
	if (IVoiceChat* VC = GetVoice())
	{
		if (bTransmitting) { VC->TransmitToAllChannels(); }
		else { VC->TransmitToNoChannels(); }
	}
#endif
}

void UAFLVoiceSubsystem::SetPeerVolume(const FString& PlayerName, float Volume)
{
#if AFLONLINE_WITH_VOICE
	if (IVoiceChat* VC = GetVoice()) { VC->SetPlayerVolume(PlayerName, FMath::Clamp(Volume, 0.f, 2.f)); }
#endif
}

bool UAFLVoiceSubsystem::IsVoiceReady() const
{
#if AFLONLINE_WITH_VOICE
	const IVoiceChat* VC = GetVoice();
	return VC && VC->IsInitialized() && VC->IsConnected() && VC->IsLoggedIn();
#else
	return false;
#endif
}

// ── afl.Voice.* console commands (dev-driven; a real Enter/IMC binding + the server-room auto-flow come later).
// Unconditional -- they only call subsystem methods (no VoiceChat types), which are no-ops on the server. ──
static UAFLVoiceSubsystem* ResolveVoice(UWorld* World) { return UAFLVoiceSubsystem::Get(World); }

static FAutoConsoleCommandWithWorldAndArgs GAFLVoiceStart(
	TEXT("afl.Voice.Start"),
	TEXT("Bring voice up + login. Arg: <localProductUserId>."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAFLVoiceSubsystem* V = ResolveVoice(World)) { V->StartVoice(Args.Num() > 0 ? Args[0] : FString()); }
	}));

static FAutoConsoleCommandWithWorldAndArgs GAFLVoiceJoin(
	TEXT("afl.Voice.Join"),
	TEXT("Join an EOS RTC room. Args: <room> <participantToken> <clientBaseUrl> [positional 0/1]."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 3) { UE_LOG(LogAFLVoice, Warning, TEXT("afl.Voice.Join <room> <token> <baseUrl> [positional]")); return; }
		const bool bPos = Args.Num() > 3 && Args[3] == TEXT("1");
		if (UAFLVoiceSubsystem* V = ResolveVoice(World)) { V->JoinRoom(Args[0], Args[1], Args[2], bPos); }
	}));

static FAutoConsoleCommandWithWorldAndArgs GAFLVoiceEcho(
	TEXT("afl.Voice.Echo"),
	TEXT("Dev mic loopback on an Echo channel. Arg: <channelName>."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAFLVoiceSubsystem* V = ResolveVoice(World)) { V->JoinEchoTest(Args.Num() > 0 ? Args[0] : TEXT("afl-echo")); }
	}));

static FAutoConsoleCommandWithWorldAndArgs GAFLVoiceLeave(
	TEXT("afl.Voice.Leave"),
	TEXT("Leave a room. Arg: <room>."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAFLVoiceSubsystem* V = ResolveVoice(World)) { V->LeaveRoom(Args.Num() > 0 ? Args[0] : FString()); }
	}));

static FAutoConsoleCommandWithWorldAndArgs GAFLVoiceMute(
	TEXT("afl.Voice.Mute"),
	TEXT("Mute/unmute the local mic. Arg: <0|1>."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAFLVoiceSubsystem* V = ResolveVoice(World)) { V->SetInputMuted(Args.Num() > 0 && Args[0] == TEXT("1")); }
	}));

static FAutoConsoleCommandWithWorldAndArgs GAFLVoiceTransmit(
	TEXT("afl.Voice.Transmit"),
	TEXT("Open-mic vs push-to-talk. Arg: <1=transmit all | 0=transmit none>."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAFLVoiceSubsystem* V = ResolveVoice(World)) { V->SetTransmitting(!(Args.Num() > 0 && Args[0] == TEXT("0"))); }
	}));

static FAutoConsoleCommandWithWorld GAFLVoiceStatus(
	TEXT("afl.Voice.Status"),
	TEXT("Log the voice provider + connection state."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UAFLVoiceSubsystem* V = ResolveVoice(World);
		UE_LOG(LogAFLVoice, Log, TEXT("AFL_VOICE status: subsystem=%d ready=%d"), V ? 1 : 0, (V && V->IsVoiceReady()) ? 1 : 0);
	}));

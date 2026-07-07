// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLPlayerIdentityComponent.h"

#include "AFLOnlineSubsystem.h"                 // UAFLOnlineSubsystem::Get / GetSessionTicket / PostServerResolve
#include "Dom/JsonObject.h"                     // resolve body build + {playFabId} parse
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"     // IsLocalController (owning-client gate)
#include "Engine/World.h"
#include "TimerManager.h"                       // canary: log the resolved id after the async round-trip
#include "Misc/DateTime.h"                      // contract ts
#include "HAL/IConsoleManager.h"                // afl.Online.ResolveCanary
#include "GameFramework/CheatManagerDefines.h"  // UE_WITH_CHEAT_MANAGER
#include "Net/UnrealNetwork.h"                  // DOREPLIFETIME

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLPlayerIdentityComponent)

DEFINE_LOG_CATEGORY_STATIC(LogAFLIdentity, Log, All);

UAFLPlayerIdentityComponent::UAFLPlayerIdentityComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// UGameFrameworkComponent has no replicated base -> enable replication or ResolvedPlayFabId never reaches the
	// owner (the "compiles but doesn't replicate" trap the wallet documents). Resolve is a one-shot -> no tick.
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLPlayerIdentityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLPlayerIdentityComponent, ResolvedPlayFabId);
}

void UAFLPlayerIdentityComponent::BeginPlay()
{
	Super::BeginPlay();
	// Client side: the OWNING client relays its SessionTicket to the server once logged in (gated below).
	TryRelayFromOwningClient();
}

void UAFLPlayerIdentityComponent::TryRelayFromOwningClient()
{
	if (bRelaySent)
	{
		return;
	}

	// Only the locally-owned PlayerState relays ITS ticket. On the server's own instance (authority) and on
	// simulated proxies of OTHER players (not local), this is false -> no relay (the wallet's local-PS gate).
	const AActor* Owner = GetOwner();
	const APlayerState* PS = Cast<APlayerState>(Owner);
	const APlayerController* PC = PS ? PS->GetPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	// The relay is for REMOTE clients (whose ticket the dedicated server lacks). The authority's own PS (a
	// listen-host player) already IS its authority and its login is locally available -> don't relay.
	if (!Owner || Owner->GetLocalRole() == ROLE_Authority)
	{
		return;
	}

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online)
	{
		return;
	}

	// Fire once the login has a ticket (immediately if already logged in).
	TWeakObjectPtr<UAFLPlayerIdentityComponent> WeakThis(this);
	Online->CallWhenLoggedIn([WeakThis, Online](bool bOk)
	{
		UAFLPlayerIdentityComponent* Self = WeakThis.Get();
		if (!Self || Self->bRelaySent || !bOk)
		{
			return;
		}
		const FString Ticket = Online->GetSessionTicket();
		if (Ticket.IsEmpty())
		{
			return;
		}
		Self->bRelaySent = true;
		Self->ServerRelaySessionTicket(Ticket);   // owning client -> server
	});
}

bool UAFLPlayerIdentityComponent::ServerRelaySessionTicket_Validate(const FString& Ticket)
{
	// Bound it: a real PlayFab session ticket is ~100-200 chars. Reject empty / absurdly large.
	return !Ticket.IsEmpty() && Ticket.Len() < 4096;
}

void UAFLPlayerIdentityComponent::ServerRelaySessionTicket_Implementation(const FString& Ticket)
{
	// AUTHORITY for THIS PlayerState. Build the contract body ({sessionTicket, ts}) and resolve it server-side via
	// the /resolve-identity Lambda. The CLIENT supplied ONLY the ticket -- the PlayFabId comes back VERIFIED from
	// PlayFab, so we never trust a client-asserted id.
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online)
	{
		UE_LOG(LogAFLIdentity, Warning, TEXT("AFL_A14 resolve SKIP -- no AFLOnline subsystem."));
		return;
	}

	const int64 Ts = FDateTime::UtcNow().ToUnixTimestamp();
	// The JSON writer escapes the ticket string properly; ts is a number (JS treats an integral value as an
	// integer, so the contract's Number.isInteger passes). Sign-what-you-send: PostServerResolve signs THIS body.
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("sessionTicket"), Ticket);
	Body->SetNumberField(TEXT("ts"), static_cast<double>(Ts));
	FString BodyStr;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body, Writer);

	TWeakObjectPtr<UAFLPlayerIdentityComponent> WeakThis(this);
	Online->PostServerResolve(BodyStr, [WeakThis](bool bOk, const FString& Resp)
	{
		UAFLPlayerIdentityComponent* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (!bOk)
		{
			UE_LOG(LogAFLIdentity, Warning, TEXT("AFL_A14 resolve FAIL %s"), *Resp.Left(300));
			return;
		}
		// Parse {playFabId} from the plain-200 body.
		FString Pid;
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			Root->TryGetStringField(TEXT("playFabId"), Pid);
		}
		if (Pid.IsEmpty())
		{
			UE_LOG(LogAFLIdentity, Warning, TEXT("AFL_A14 resolve FAIL -- 200 but no playFabId: %s"), *Resp.Left(300));
			return;
		}
		Self->ResolvedPlayFabId = Pid;   // server-authoritative write (replicates to the owner)
		UE_LOG(LogAFLIdentity, Log, TEXT("AFL_A14 resolve ok pid=%s"), *Pid);
	});
}

#if UE_WITH_CHEAT_MANAGER
// ─── A1.4 LIVE identity canary: afl.Online.ResolveCanary ───────────────────────────────────────────────────
// Force-relays the LOCAL player's SessionTicket through the identity component's resolve path (in editor the
// local PS IS the authority, so the RPC runs the resolve locally), then logs AFL_A14_CANARY pid=<verified id>
// after the async round-trip. Green (pid=DF7C3188377BB66D for AFL_DEV_TEST_01) proves the full
// client->server->Lambda->PlayFab identity path end-to-end BEFORE any consumer reads it. Needs AFL_RESOLVE_URL +
// AFL_EARN_HMAC_KEY in the env (as the earn canary), and the identity component registered on the PlayerState.
static void HandleAFLResolveCanary(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAFLPlayerIdentityComponent* Id = PS ? PS->FindComponentByClass<UAFLPlayerIdentityComponent>() : nullptr;
	if (!Id)
	{
		Ar.Log(TEXT("afl.Online.ResolveCanary - no identity component on the local PlayerState. Register UAFLPlayerIdentityComponent via the AFLCombat AddComponents action + run on an experience that grants it."));
		return;
	}

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(World);
	const FString Ticket = Online ? Online->GetSessionTicket() : FString();
	if (Ticket.IsEmpty())
	{
		Ar.Log(TEXT("afl.Online.ResolveCanary - no local SessionTicket (not logged in yet)."));
		return;
	}

	Ar.Logf(TEXT("afl.Online.ResolveCanary -> relaying ticket (len=%d) to /resolve-identity..."), Ticket.Len());
	Id->ServerRelaySessionTicket(Ticket);   // editor authority: runs the resolve locally

	// Echo the stored id under the canary marker once the async resolve lands (the AFL_A14 resolve ok/FAIL line
	// prints from the completion itself).
	if (World)
	{
		TWeakObjectPtr<UAFLPlayerIdentityComponent> WeakId(Id);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, [WeakId]()
		{
			const UAFLPlayerIdentityComponent* C = WeakId.Get();
			UE_LOG(LogAFLIdentity, Log, TEXT("AFL_A14_CANARY pid=%s"), C ? *C->GetResolvedPlayFabId() : TEXT("(gone)"));
		}, 2.5f, false);
	}
}

FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLResolveCanaryCmd(TEXT("afl.Online.ResolveCanary"),
	TEXT("A1.4 LIVE identity canary (dedicated server OR editor PIE): relay the local player's SessionTicket to /resolve-identity and log AFL_A14_CANARY pid=<verified PlayFabId>. Green = the UE-relayed ticket resolves to the real account (DF7C3188377BB66D for AFL_DEV_TEST_01), proving client->server->Lambda->PlayFab end-to-end. Needs AFL_RESOLVE_URL + AFL_EARN_HMAC_KEY env vars set, and UAFLPlayerIdentityComponent registered on the PlayerState (AFLCombat AddComponents)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLResolveCanary));
#endif // UE_WITH_CHEAT_MANAGER

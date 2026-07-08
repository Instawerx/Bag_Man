// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/PlayerStateComponent.h"

#include "AFLPlayerIdentityComponent.generated.h"

/**
 * UAFLPlayerIdentityComponent -- A1.4: server-authoritative per-player PlayFab identity (sibling to the wallet).
 *
 * THE GAP IT CLOSES: on a DEDICATED server, UAFLOnlineSubsystem holds only the SERVER's own login, so
 * MakePlayerId()/GetPlayFabId() cannot yield a remote player's PlayFabId (every earn would grant to the server's
 * account). This component gets each player's REAL PlayFabId to the server WITHOUT trusting a client-asserted id:
 * the OWNING client relays its PlayFab SessionTicket (PlayFab-bound, unforgeable for another account) via a
 * Server RPC; the server signs+POSTs it to the /resolve-identity Lambda (Server/AuthenticateSessionTicket, title
 * secret Lambda-only) and stores the VERIFIED PlayFabId it returns. The client supplies ONLY the ticket -- the id
 * is server-DERIVED. Step-3's earn hook (and, later, MakePlayerId) read GetResolvedPlayFabId().
 *
 * WITHIN LYRA: a UPlayerStateComponent, same shape + registration as UAFLWalletComponent -- added to the
 * ALyraPlayerState via the AFLCombat GameFeature AddComponents action (Plugins/GameFeatures/AFLCombat/Content/
 * AFLCombat.uasset).
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLPlayerIdentityComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	// UPlayerStateComponent has no default ctor (only the FObjectInitializer overload) -> forward to Super.
	UAFLPlayerIdentityComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The server-verified PlayFabId for THIS player (empty until resolved). Server-authoritative; the owner
	 *  reads its own (replicated) copy but can NEVER write it. Step-3 earn hook + MakePlayerId (later) read this. */
	UFUNCTION(BlueprintPure, Category = "AFL|Identity")
	FString GetResolvedPlayFabId() const { return ResolvedPlayFabId; }

	/** Owning client -> server: relay the player's PlayFab SessionTicket. The server verifies it via the
	 *  /resolve-identity Lambda and sets ResolvedPlayFabId (server-derived, NEVER client-asserted). Mirrors the
	 *  wallet's per-player Server-RPC shape (AFLWalletComponent.h:114). */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRelaySessionTicket(const FString& Ticket);

private:
	/** Server-set ONLY (in the resolve completion). Replicated server->client so the owner may read its own id;
	 *  there is NO client setter -> anti-spoof (the client supplies only the verifiable ticket). */
	UPROPERTY(Replicated)
	FString ResolvedPlayFabId;

	/** Client-side one-shot: on the LOCALLY-OWNED PlayerState, fire the relay once login has a ticket. No-op on
	 *  the server's own instance and on simulated proxies of other players. */
	void TryRelayFromOwningClient();

	/** Client: relay exactly once. */
	bool bRelaySent = false;

	/** Server: afl.Test.AutoEarnOnResolve dev-test hook fires the REAL earn funnel exactly once per resolved player (headless assertion-4 driver). */
	bool bTestAutoEarnFired = false;
};

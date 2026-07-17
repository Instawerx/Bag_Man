// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "AFLReconcileIdComponent.generated.h"

/**
 * UAFLReconcileIdComponent  (Team SSOT §3 -- the T2 identity-join stash)
 *
 * Holds the reconcile key (the PlayFab entity id) a connecting client carries in its ?PlayFabId= connect option.
 * Added to the player's PlayerState server-side at AAFLGameMode::InitNewPlayer, and read by
 * UAFLMatchmakerDataProvider to reconcile the GameLift matchmaker roster (member.id) against the actual connected
 * controllers. Server-authoritative only (the reconcile runs on the server) -> NOT replicated. Absent for
 * LocalFill / offline / PIE joins (no ?PlayFabId=), where the matchmaker path is inert anyway.
 *
 * Built + wired in T2 Increment 2; exercised live only once the online path lands (S12). Delete-safe with S12 if
 * the roster is instead read from a different per-player carrier.
 */
UCLASS()
class AFLGAMECORE_API UAFLReconcileIdComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	const FString& GetReconcileId() const { return ReconcileId; }
	void SetReconcileId(const FString& InReconcileId) { ReconcileId = InReconcileId; }

private:
	/** The PlayFab entity id (the identity-join key). Server-set at InitNewPlayer; not replicated. */
	UPROPERTY()
	FString ReconcileId;
};

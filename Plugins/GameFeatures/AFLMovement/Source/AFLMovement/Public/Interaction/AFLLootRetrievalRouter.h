// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "AFLLootRetrievalRouter.generated.h"

/**
 * How a grabber should RETRIEVE a piece of loot -- resolved by the loot's owner-relationship and queried by the
 * grab ability BEFORE it commits to a mechanism (Loot-Carry Phase C3). This lets ONE interaction (the grab) route
 * to TWO mechanisms by actor-relationship WITHOUT deciding too early: the static GrabKind flag is read before
 * owner-vs-enemy is known, so it could only pick channel-vs-instant blindly. This moves that fork downstream of
 * the relationship -- owner stays instant + untouched, enemy routes to the carry-model collect-channel.
 */
UENUM()
enum class EAFLRetrievalMode : uint8
{
	/** The grabber OWNS this loot -> the existing INSTANT hand-grab path (dismember: reattach). NO channel. */
	OwnerReattach,
	/** An eligible NON-owner -> the carry-model COLLECT-CHANNEL (-> carried pool, then despawn). */
	EnemyCollect,
	/** Not eligible to retrieve (e.g. a TeamOnly cache grabbed off-team) -> no grab. */
	Ineligible
};

UINTERFACE(MinimalAPI)
class UAFLLootRetrievalRouter : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by a loot object's grant component (UAFLLootGrantComponent, AFLCombat). The grab ability
 * (AFLMovement) finds it on the target BY INTERFACE (no hard cast, per ue5-interaction-ik-expert) and routes the
 * mechanism on the returned relationship. The owner-vs-enemy distinction resolves ONCE (the grant's SSOT) and
 * drives BOTH this pre-mechanism routing AND the downstream value destination in TryGrant -- no duplicate match.
 */
class IAFLLootRetrievalRouter
{
	GENERATED_BODY()

public:
	/** Resolve how Grabber should retrieve this loot (owner-vs-enemy), reusing the grant's resolution. */
	virtual EAFLRetrievalMode ResolveRetrievalMode(const AActor* Grabber) const = 0;
};

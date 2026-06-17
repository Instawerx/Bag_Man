// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "AFLLootable.generated.h"

class UAFLLootGrantComponent;

UINTERFACE(MinimalAPI, BlueprintType)
class UAFLLootable : public UInterface
{
	GENERATED_BODY()
};

/**
 * IAFLLootable -- the marker contract every loot object honors, regardless of base class (head/limb derive
 * from AAFLDismemberedPart; energy from AActor; future caches/nodes from a new base). An INTERFACE (not a
 * base class) so nothing reparents -- any system (the Phase-3 loot director, a query, a UI) can
 * Cast<IAFLLootable>(Actor) and reach its UAFLLootGrantComponent without knowing the concrete type.
 *
 * Lean by design: the grant component (UAFLLootGrantComponent) is the workhorse; this interface just
 * exposes it polymorphically. Caches/nodes implement it in their phase; head/limb implement it now.
 */
class AFLCOMBAT_API IAFLLootable
{
	GENERATED_BODY()

public:
	/** The loot object's grant component (the eligibility + grant-once + value grant). May be null if the
	 *  implementer has not created one. */
	virtual UAFLLootGrantComponent* GetLootGrantComponent() const = 0;
};

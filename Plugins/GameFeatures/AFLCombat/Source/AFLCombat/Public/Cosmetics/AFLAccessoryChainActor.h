// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Cosmetics/AFLAccessoryPartActor.h"

#include "AFLAccessoryChainActor.generated.h"

class UChildActorComponent;
class USkeletalMeshComponent;

/**
 * CC-8: the CHAIN part actor. A chain OWNS its pendant, per the ruling -- the pendant is not a peer
 * character part on the pawn, it is spawned BY this actor onto THIS actor's own mesh at
 * accessory_pendant.
 *
 * WHY THE CHAIN OWNS IT, restated because the whole design turns on it: AddCharacterPart attaches to the
 * PAWN's mesh, and a spawned part is not the pawn, so there is no engine path to hang a pendant on a
 * chain through the customizer. The chain reads the pendant id itself and spawns it as its own child.
 *
 * THE CONSEQUENCES ARE STRUCTURAL, NOT ENFORCED. No chain equipped -> no chain actor -> nowhere for a
 * pendant to live. Un-equip the chain -> this actor is destroyed -> its pendant child dies with it. The
 * SELECTION is never touched -- FAFLAccessorySet still holds the pendant id -- so re-equipping the chain
 * spawns a fresh chain actor that reads the same id and restores the pendant as the player left it.
 *
 * THE PENDANT RIDES accessory_pendant ON THE LOWEST SIMULATED BONE. Authored there in the conform
 * (chain_04). A socket on a static bone would leave the pendant hanging motionless inside a swaying
 * chain; on the lowest simulated bone the pendant sways WITH the chain because it is a child of that bone.
 */
UCLASS(Abstract)
class AFLCOMBAT_API AAFLAccessoryChainActor : public AAFLAccessoryPartActor
{
	GENERATED_BODY()

public:
	AAFLAccessoryChainActor();

	/** Re-read the player's pendant selection and (re)spawn it on this chain's mesh. Idempotent: removes
	 *  a previously-spawned pendant before spawning again, so a pendant swap while the chain is worn does
	 *  not stack. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Accessory")
	void RefreshPendant();

	/** True once a pendant is spawned and attached. Lets a test assert the dependency resolved rather
	 *  than inferring it from a transform. */
	UFUNCTION(BlueprintPure, Category = "AFL|Accessory")
	bool HasPendant() const { return SpawnedPendant != nullptr; }

protected:
	virtual void BeginPlay() override;

	/** The socket on THIS chain's mesh where the pendant hangs. Matches the conform. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Accessory")
	FName PendantSocket = FName(TEXT("accessory_pendant"));

private:
	/** Walk owner pawn -> PlayerState -> loadout, return the equipped pendant CosmeticId or NAME_None. */
	FName ResolvePendantId() const;

	/** This chain's own skeletal mesh, found on the BP-authored components. */
	USkeletalMeshComponent* FindChainMesh() const;

	UPROPERTY(Transient)
	TObjectPtr<UChildActorComponent> SpawnedPendant = nullptr;

	UPROPERTY(Transient)
	FName SpawnedPendantId;
};

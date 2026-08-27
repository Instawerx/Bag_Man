// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AFLSurfaceProviderInterface.generated.h"

class USkeletalMeshComponent;

/**
 * WHERE IS THE VISIBLE SURFACE? A modular pawn cannot answer that from its own mesh.
 *
 * CharacterMesh0 is SKM_Manny_Invis -- it carries collision and draws nothing. The body the player
 * sees arrives at runtime through Lyra's AddCharacterPart as a child actor, and the roster swaps it.
 * So the silhouette an accessory must sit on is not a constant and is not the pawn's own mesh.
 *
 * MEASURED, and this is why the interface exists rather than a direct GetMesh() call: line traces
 * aimed at the accessory sockets stopped on CharacterMesh0 and on CollisionCylinder -- the invisible
 * base and the capsule -- while every accessory was reported drawn, correctly placed and correctly
 * sized. Both blockers answer for a surface that is never rendered. Anything that pins jewellery by
 * querying the pawn will pin it to the wrong boundary.
 *
 * Implementers return the mesh that is actually DRAWN for a given slot, or nullptr. Returning the
 * invisible base is a defect, not a fallback.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UAFLSurfaceProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class IAFLSurfaceProviderInterface
{
	GENERATED_BODY()

public:
	/**
	 * The visible skeletal mesh a piece in this slot must conform to.
	 *
	 * SlotName is the surface KEY, not the accessory socket:
	 *   "Body"  -> the equipped body character part's mesh (wrists and neck fit to this)
	 *   "Neck"  -> the equipped CHAIN's mesh (the pendant fits to this)
	 *
	 * The pendant case is structural, not a special case: a pendant hangs from a chain, and
	 * AddCharacterPart can only ever attach to the pawn's mesh
	 * (ULyraPawnComponent_CharacterParts::GetSceneComponentToAttachTo returns
	 * Cast<ACharacter>(Owner)->GetMesh()). So the chain spawns the pendant onto its own mesh, and the
	 * surface the pendant fits is the chain.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|AccessoryIK")
	USkeletalMeshComponent* GetVisibleMeshForSlot(FName SlotName);

	/**
	 * Publish the visible mesh for a slot. Called from the SPAWN pipelines, which are asymmetric:
	 * body parts arrive through Lyra's AddCharacterPart, the pendant through the chain actor's own
	 * RefreshPendant. Registration must therefore happen at two places, not one.
	 *
	 * Implementers MUST refuse the pawn's own mesh -- that is the invisible base, and accepting it
	 * silently pins every piece to a boundary nothing renders.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|AccessoryIK")
	void RegisterMeshForSlot(FName SlotName, USkeletalMeshComponent* VisibleMesh);
};

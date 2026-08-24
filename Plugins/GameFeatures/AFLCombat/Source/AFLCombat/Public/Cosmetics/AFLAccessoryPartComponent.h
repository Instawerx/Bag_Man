// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"
#include "Cosmetics/AFLCosmeticSelectionTypes.h"
#include "Cosmetics/LyraCharacterPartTypes.h"

#include "AFLAccessoryPartComponent.generated.h"

class APawn;
class UActorComponent;

/**
 * CC-8: THE ACCESSORY CONSUMER. Closes CC-X37 -- FAFLAccessorySet was written, replicated and refused
 * correctly, and nothing read it to attach a mesh.
 *
 * THE BODY-SELECTOR'S SIBLING, deliberately. UAFLCharacterPartSelectorComponent resolves ONE body on
 * possession; this resolves N accessories and re-resolves whenever the selection changes. Same
 * controller-component shape, same reflected AddCharacterPart call, same reason for the reflection:
 * ULyraControllerComponent_CharacterParts carries no LYRAGAME_API export, so a direct cross-module call
 * is LNK2019 and the UFUNCTION thunk is the only way in.
 *
 * THREE SLOTS, NOT FOUR. Neck, WristLeft and WristRight are ordinary sockets on the PAWN's skeleton and
 * ride AddCharacterPart. PENDANT DOES NOT -- it is spawned by the CHAIN'S part actor onto the chain's own
 * mesh, because AddCharacterPart attaches to the pawn's mesh and a spawned part is not the pawn. That is
 * a fact about the engine API, not a choice:
 *
 *     FLyraCharacterPartList::SpawnActorForEntry ->
 *       PartComponent->SetupAttachment(OwnerComponent->GetSceneComponentToAttachTo(), SocketName)
 *     ULyraPawnComponent_CharacterParts::GetSceneComponentToAttachTo ->
 *       Cast<ACharacter>(OwnerActor)->GetMesh()   // always the pawn, never a part
 *
 * The consequences of the chain owning the pendant are STRUCTURAL rather than enforced, which is why
 * there is no code here for them: no chain means no chain part, so there is nowhere for a pendant to
 * live; un-equipping a chain destroys the chain part and the pendant goes with it. The SELECTION is
 * untouched in either case -- FAFLAccessorySet still holds the pendant, so re-equipping the chain
 * re-spawns both, as the player left them.
 *
 * REMOVES BY VALUE, NEVER CLEAR-ALL. AddCharacterPart appends, so re-resolving without removing stacks
 * duplicates. RemoveAllCharacterParts would take the BODY with it -- the body selector and this component
 * share one stock component on the controller. Each side removes exactly what it added.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLAccessoryPartComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	/**
	 * AUTHORITY: read the player's FAFLAccessorySet and make the pawn's character-part list match it.
	 * Idempotent -- removes what it previously added before adding again, so a re-drive on every
	 * selection change cannot stack parts.
	 */
	void RefreshAccessoriesForPawn(APawn* Pawn);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

private:
	/** Walks the controller's components for the stock ULyraControllerComponent_CharacterParts. Matched on
	 *  the SUPER-CHAIN name: the component is a BP subclass (B_BagMan_AssignCharacterPart_C), so a leaf-name
	 *  match misses, and the unexported base cannot be IsA<>'d at compile time. */
	UActorComponent* FindStockPartsComponent() const;

	/** The parts THIS component put on the list, so it can take exactly those off again. Keyed by slot so a
	 *  wrist swap replaces one side without disturbing the other. */
	UPROPERTY(Transient)
	TMap<uint8, FLyraCharacterPart> AddedParts;
};

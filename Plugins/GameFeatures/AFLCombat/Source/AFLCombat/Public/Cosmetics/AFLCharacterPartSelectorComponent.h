// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "AFLCharacterPartSelectorComponent.generated.h"

class APawn;
class AActor;
class UAFLCharacterPartMap;

/**
 * Controller-side selector for WHICH robot BODY a player wears (Phase 0 keystone wire).
 *
 * The body-axis sibling of UAFLSkinColorControllerComponent: that component resolves the per-pawn EDGE
 * COLOR on possession and pushes it onto the pawn's UAFLSkinColorComponent; THIS component resolves the
 * per-player IDENTITY (FAFLCosmeticSelection::GetActiveIdentityId(), read from the PlayerState's
 * UAFLCosmeticLoadoutComponent) to a robot CharacterPart class (B_AFL_Robot_<Name>_C) and ADDS it on
 * possession via the pawn's ULyraPawnComponent_CharacterParts::AddCharacterPart.
 *
 * WHY THIS REPLACES THE HARDCODED PIN: today B_BagMan_AssignCharacterPart (a
 * ULyraControllerComponent_CharacterParts) carries a SINGLE static CharacterParts[0]=B_AFL_Robot_ARIA
 * entry applied to EVERY controller -> everyone is ARIA. This selector picks the body per player from the
 * proven #43 selection. Once it ships, the static ARIA entry on B_BagMan_AssignCharacterPart is removed so
 * this is the sole body-add.
 *
 * Lives on the Controller -> survives pawn death; re-resolves on each (re)possession (the same
 * OnPossessedPawnChanged hook + respawn-race PlayerState fallback the color sibling uses). Standalone
 * subclass of the engine UControllerComponent (exported); we do NOT subclass the module-private
 * ULyraControllerComponent_CharacterParts. The proven color-swap path is UNTOUCHED -- this only chooses
 * which body class spawns; SetSkinColor/ApplySkinColor recolor it on top exactly as before.
 *
 * Granted to the player Controller via GameFeatureAction_AddComponents in the experience (the same
 * mechanism as UAFLSkinColorControllerComponent).
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLCharacterPartSelectorComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	/** AUTHORITY: resolve this pawn's identity (PlayerState loadout selection) to a robot body class and add
	 *  it to the pawn's customizer. The possess hook calls THIS. Idempotent-by-intent: it adds the resolved
	 *  body once per possession (a fresh pawn starts with no parts; re-possession gets a fresh pawn). */
	void ResolveBodyForPawn(APawn* Pawn) const;

protected:
	virtual void BeginPlay() override;

	/** Identity id (AFL.Team.<Name> / AFL.Character.<Name>) -> robot body CharacterPart class. The body-axis
	 *  analogue of the color sibling's BrandEdgeMap. EditDefaultsOnly: the GameFeatureAction-added component
	 *  carries this as a per-class default (set to DA_AFL_CharacterPartMap in the details panel). On a miss
	 *  (unset id / unmapped / null map) the resolver falls back to FallbackPartClass below. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Cosmetics")
	TObjectPtr<UAFLCharacterPartMap> PartMap = nullptr;

	/** The default robot body when no identity resolves (preserves the prior hardcoded behavior). Set to
	 *  B_AFL_Robot_ARIA_C in the details panel. EditDefaultsOnly: a per-class default on the added component.
	 *  Soft so the default robot BP isn't force-loaded until a fallback actually occurs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Cosmetics")
	TSoftClassPtr<AActor> FallbackPartClass;

	/** Bound (authority) to the controller's public OnPossessedPawnChanged; resolves+adds the body for the
	 *  new pawn. Mirrors UAFLSkinColorControllerComponent::OnPossessedPawnChanged. */
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

private:
	/** Read the active identity id from the pawn's PlayerState loadout (pawn-side PS first, controller-side
	 *  fallback -- the respawn-race fix proven in RefreshSkinForPawn). NAME_None if no loadout / unset. */
	FName ResolveIdentityId(APawn* Pawn) const;
};

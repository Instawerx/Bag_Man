// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"

#include "AFLSkinColorControllerComponent.generated.h"

class APawn;
class UAFLSkinColorAsset;

/**
 * Controller-side PERSISTENT home for the robot-skin color selection (L5, Option F).
 *
 * Lives on the Controller -> survives pawn death (unlike the pawn's UAFLSkinColorComponent, which dies
 * with the pawn). On each possession it re-pushes PersistentSkinColor onto the new pawn's
 * UAFLSkinColorComponent (authority) -> sets the replicated SkinColor -> all clients converge. This is
 * what makes the skin survive respawn. Standalone subclass of the engine UControllerComponent (exported);
 * we do NOT subclass the module-private ULyraControllerComponent_CharacterParts.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLSkinColorControllerComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	/** AUTHORITY-ONLY: set the persistent color (survives respawn) + push to the current pawn now. */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "AFL|Cosmetics")
	void SetPersistentSkinColor(UAFLSkinColorAsset* NewColor);

	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics")
	UAFLSkinColorAsset* GetPersistentSkinColor() const { return PersistentSkinColor; }

protected:
	virtual void BeginPlay() override;

	/** Authority-side selection state. NOT replicated -- the pawn component replicates the active value.
	 *  EditDefaultsOnly so a per-component-class default color can be authored in the details panel (the
	 *  GameFeatureAction-added component carries this default; the gate uses it for ARIA = Green). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Cosmetics")
	TObjectPtr<UAFLSkinColorAsset> PersistentSkinColor = nullptr;

	/** Bound (authority) to the controller's public OnPossessedPawnChanged; re-pushes color to the new pawn. */
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

private:
	void PushToPawn(APawn* Pawn) const;
};

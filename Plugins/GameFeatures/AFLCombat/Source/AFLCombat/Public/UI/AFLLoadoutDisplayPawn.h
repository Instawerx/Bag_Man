// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Character.h"

#include "AFLLoadoutDisplayPawn.generated.h"

class UAFLSkinColorComponent;
class USkeletalMesh;
class UAnimInstance;

/**
 * AAFLLoadoutDisplayPawn -- an ASC-LESS, non-replicated DISPLAY pawn for the loadout / armory preview.
 *
 * THE KEY UNLOCK: cosmetics need NO GAS -- every axis is a material push (skin/body/edge/beam), a
 * character-part add (identity robot), a slot-1 material swap (facemask), or a Lyra equipment equip
 * (weapon). None of it touches the AbilitySystemComponent. So this pawn shows the player's full cosmetic
 * loadout WITHOUT the combat stack, and -- being ASC-less -- dodges the ASC-gated AFLCombat ability grant.
 *
 * It is NOT the gameplay hero. The loadout captures THIS (a spawned display instance) instead of
 * PC->GetPawn(), so the pod preview works with no live gameplay pawn (the front-end bug's fix under B).
 *
 * bReplicates=false is LOAD-BEARING: a local non-replicated actor has HasAuthority()==true even on a
 * client, so UAFLSkinColorComponent::SetSkinColor/etc. (BlueprintAuthorityOnly) apply DIRECTLY to it.
 *
 * Minimal display set: an ACharacter (the character-parts comp attaches to ACharacter::GetMesh()) whose
 * invisible driving mesh (SKM_Manny_Invis) is copy-posed by the spawned robot part; a reflection-added
 * ULyraPawnComponent_CharacterParts (module-private in LyraGame); UAFLSkinColorComponent; and a
 * ULyraEquipmentManagerComponent for the weapon visual. NO ASC, NO combat components, NO ability sets.
 */
UCLASS(Blueprintable)
class AFLCOMBAT_API AAFLLoadoutDisplayPawn : public ACharacter
{
	GENERATED_BODY()

public:
	AAFLLoadoutDisplayPawn(const FObjectInitializer& ObjectInitializer);

	/** Pawn-side skin/body/edge/facemask/beam holder -- the proven fan-out (Refresh*ForPawn) pushes here. */
	UAFLSkinColorComponent* GetSkinColorComponent() const { return SkinColorComp; }

	/** The reflection-added ULyraPawnComponent_CharacterParts (module-private -> found by base-class-name walk). */
	UActorComponent* FindCharacterPartsComponent() const;

	/** Add a robot body CharacterPart to THIS pawn's part comp via reflection (the identity/IRONICS-fallback
	 *  body). Mirrors UAFLCharacterPartSelectorComponent's AddCharacterPart reflection, but targets the DISPLAY
	 *  pawn's own pawn-side comp (the selector's ResolveBodyForPawn targets the CONTROLLER's possessed pawn, so
	 *  it can't be reused for a display pawn). Remove-then-add = idempotent replace. */
	void SetRobotBody(TSubclassOf<AActor> RobotPartClass);

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/** (Re)apply the invisible driving mesh to GetMesh() + force always-tick. MUST run after SetRobotBody too:
	 *  the character-parts add/remove resets the owner mesh to null, which kills the weapon sockets + copy-pose. */
	void ApplyDrivingMesh();

	/** The invisible driving skeletal mesh the robot part copy-poses from (SKM_Manny_Invis). Overridable. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Display")
	TSoftObjectPtr<USkeletalMesh> DrivingMesh;

	/** Optional idle AnimBP for the driving mesh; unset -> ref-pose (acceptable for the de-risk slice). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Display")
	TSoftClassPtr<UAnimInstance> DrivingAnimClass;

private:
	UPROPERTY(VisibleAnywhere, Category = "AFL|Display")
	TObjectPtr<UAFLSkinColorComponent> SkinColorComp;
};

// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "AFLCharacterPartMap.generated.h"

class AActor;

/**
 * CharacterId -> body-part class mapping for the BagMan robots (Phase 0 keystone wire).
 *
 * A pure, typed lookup -- the identity sibling of UAFLBrandEdgeMap. Where BrandEdgeMap maps a
 * Cosmetic.Brand.* tag to a default edge COLOR preset, this maps a player's IDENTITY id
 * (FAFLCosmeticSelection::GetActiveIdentityId(), format AFL.Character.<Name> / AFL.Team.<Name>) to the
 * robot BODY CharacterPart class (B_AFL_Robot_<Name>_C, a child of AAFLCharacterPartActor) that the
 * controller's part-selector spawns for that player.
 *
 * WHY THIS EXISTS: the visible robot is added on the controller's ULyraControllerComponent_CharacterParts
 * (B_BagMan_AssignCharacterPart) -- today a SINGLE static CharacterParts[0]=B_AFL_Robot_ARIA entry applied
 * to EVERY controller. This map is the missing piece that lets the resolver pick WHICH body per player from
 * the proven #43 selection (UAFLCosmeticLoadoutComponent on the PlayerState). It changes ONLY which body
 * class is chosen; the proven color-swap path (SetSkinColor / ApplySkinColor) is UNTOUCHED -- every robot
 * still recolors via the existing edge system on top of whichever body this map selects.
 *
 * TSoftClassPtr (not a hard TObjectPtr like BrandEdgeMap's preset): the robot BP classes are content and a
 * player can hold an identity key whose body BP isn't currently loaded; the resolver hard-loads on demand at
 * possess. ZERO reflection (mirrors UAFLBrandEdgeMap / UAFLSkinColorAsset): a C++-typed TMap + a direct
 * getter, so resolution reads it with no FindFProperty / runtime reflection lookup that could silently miss.
 */
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLCharacterPartMap : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Identity id (AFL.Character.<Name> / AFL.Team.<Name>) -> the robot body CharacterPart class
	 *  (B_AFL_Robot_<Name>_C) to spawn for that identity. An identity with no entry resolves to none
	 *  (the resolver then falls back to its default body, preserving current behavior). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Cosmetic", meta = (AllowAbstract = "false"))
	TMap<FName, TSoftClassPtr<AActor>> IdentityToPart;

	/** Resolve the robot body class for an identity id; an empty soft-class if unmapped (caller falls back
	 *  to its default). Mirrors UAFLBrandEdgeMap::ResolveEdge -- pure typed Find, no reflection, miss = empty. */
	TSoftClassPtr<AActor> ResolveCharacterPart(const FName& IdentityId) const
	{
		const TSoftClassPtr<AActor>* Found = IdentityToPart.Find(IdentityId);
		return Found ? *Found : TSoftClassPtr<AActor>();
	}
};

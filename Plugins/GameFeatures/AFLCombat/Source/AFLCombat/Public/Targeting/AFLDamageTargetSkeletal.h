// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"

#include "AFLDamageTargetSkeletal.generated.h"

class USkeletalMeshComponent;
class ULyraAbilitySystemComponent;
class UAFLAttributeSet_Combat;

/**
 * Skeletal sibling of AAFLDamageTarget. Identical ASC + CombatSet + overkill
 * wiring, but with a USkeletalMeshComponent (SKM_Manny + PA_Mannequin assigned
 * at the BP layer, B_AFL_DamageTarget_Skel) so hits carry FHitResult.BoneName.
 * The static cube target reports BoneName=NAME_None (no bones); this one lets
 * the dismember head-zone path (S4-04) actually fire on a real "head" bone.
 *
 * Deliberately a plain AActor, NOT ALyraCharacter -- same reason as
 * AAFLDamageTarget: dodges BM-DEBT-004's AddAbilities CastChecked crash on
 * unpossessed Lyra pawns placed in a map.
 *
 * NOTE: mirrors AAFLDamageTarget's wiring (BM-0102b). If a third target variant
 * ever appears, extract a shared base then -- not worth the refactor risk to the
 * working static target for the second variant.
 */
UCLASS(Blueprintable)
class AFLCOMBAT_API AAFLDamageTargetSkeletal : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAFLDamageTargetSkeletal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;

	//~ IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Target", meta = (AllowPrivateAccess = true))
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Target", meta = (AllowPrivateAccess = true))
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<const UAFLAttributeSet_Combat> CombatSet;
};

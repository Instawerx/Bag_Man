// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "AFLObjectClassAnimSet.generated.h"

class UAnimMontage;

/** Per-class animation payload for a grabbable (4e dispatch substrate).
 *  A grabbable declares its "class" by pointing FAFLGrabPolicy::ObjectAnimSet at one shared
 *  asset (N heads -> one OCAS_Head). InteractionComponent resolves+caches it at grab-begin.
 *  Pose/montage/IK consumption is 4f; 4e only resolves + logs. */
UCLASS(BlueprintType)
class AFLMOVEMENT_API UAFLObjectClassAnimSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Anim")
	FGameplayTag ObjectClass;                              // identity tag, shown in 4e readout

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Anim")
	TSoftObjectPtr<UAnimMontage> CarryPose;                // authored later; consumed in 4f

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Anim")
	TSoftObjectPtr<UAnimMontage> GrabReachMontage;         // authored later; consumed in 4f (reach-then-attach)
};

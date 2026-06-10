// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"

#include "AFLAnimNotify_GameplayEvent.generated.h"

/**
 * UAFLAnimNotify_GameplayEvent  (reusable anim -> GAS bridge, Cycle 4f)
 *
 * Fires a GameplayEvent at the mesh's owning actor when the notify is hit. This is the generic seam
 * between authored animation timing and ability logic: place it on a montage at the frame where gameplay
 * should react, and any ability listening for EventTag on that actor (WaitGameplayEvent) reacts. First
 * use: Event.Interaction.GrabAttach at the grab-reach montage's hand-contact frame -- the grab ability
 * attaches the object AT contact instead of at button-press (reach-then-attach). Carries no gameplay
 * itself: no trace, no damage, no cosmetics -- just the event.
 */
UCLASS(meta = (DisplayName = "AFL Gameplay Event"))
class AFLMOVEMENT_API UAFLAnimNotify_GameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Event routed to the mesh owner's ability system when the notify fires. */
	UPROPERTY(EditAnywhere, Category = "AFL|Event")
	FGameplayTag EventTag;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};

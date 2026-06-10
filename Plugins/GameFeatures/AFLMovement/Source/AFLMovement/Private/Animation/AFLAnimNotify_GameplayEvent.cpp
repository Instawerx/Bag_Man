// Copyright C12 AI Gaming. All Rights Reserved.

#include "Animation/AFLAnimNotify_GameplayEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAnimNotify_GameplayEvent)

void UAFLAnimNotify_GameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// Null-safe by design: editor preview meshes and tagless placements hit notifies too -- do nothing.
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Owner || !EventTag.IsValid())
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.OptionalObject = this; // lets a listener identify which notify asset fired, if it cares.
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, EventData);
}

FString UAFLAnimNotify_GameplayEvent::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Event: %s"), *EventTag.ToString());
}

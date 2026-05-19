// Copyright C12 AI Gaming. All Rights Reserved.

#include "Input/AFLInputConfig_Combat.h"

#include "GameplayTagContainer.h"
#include "Input/AFLInputAction_Fire.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLInputConfig_Combat)


UAFLInputConfig_Combat::UAFLInputConfig_Combat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Construct the Fire input action as a default subobject so the
	// AbilityInputActions entry below points to a valid UInputAction the moment
	// the asset's CDO finishes constructing. NewObject is illegal inside a
	// UObject constructor; CreateDefaultSubobject is the supported path.
	FireInputAction = ObjectInitializer.CreateDefaultSubobject<UAFLInputAction_Fire>(
		this, TEXT("FireInputAction"));

	FLyraInputAction FireEntry;
	FireEntry.InputAction = FireInputAction;
	FireEntry.InputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Fire"));
	AbilityInputActions.Add(FireEntry);
}

const UInputAction* UAFLInputConfig_Combat::FindAbilityInputActionForTag(const FGameplayTag& InputTag) const
{
	for (const FLyraInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && (Action.InputTag == InputTag))
		{
			return Action.InputAction;
		}
	}
	return nullptr;
}

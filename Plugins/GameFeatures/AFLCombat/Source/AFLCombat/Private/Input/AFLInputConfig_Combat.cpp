// Copyright C12 AI Gaming. All Rights Reserved.

#include "Input/AFLInputConfig_Combat.h"

#include "GameplayTagContainer.h"
#include "Input/AFLInputAction_Fire.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLInputConfig_Combat)

// Native tag — InputTag.Weapon.Fire is declared in AFLCore's ini and used
// here at CDO construction. Even though AFLCore loads before AFLCombat,
// declaring the tag natively here is defensive: it removes the cross-plugin
// load-order dependency and matches the pattern used elsewhere in this
// plugin (see AFLAG_Laser_Pulse.cpp for the rationale that proved necessary).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputTag_Weapon_Fire, "InputTag.Weapon.Fire");


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
	FireEntry.InputTag = TAG_InputTag_Weapon_Fire;
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

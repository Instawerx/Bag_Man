// Copyright C12 AI Gaming. All Rights Reserved.

#include "Interaction/AFLGrabbableComponent.h"

#include "Interaction/InteractionOption.h"
#include "Interaction/InteractionQuery.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGrabbableComponent)

UAFLGrabbableComponent::UAFLGrabbableComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // pure marker + policy holder; no tick.
}

void UAFLGrabbableComponent::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder)
{
	// Already carried -> offer nothing (a second instigator's discovery rejects it; prevents double-grab).
	if (bHeld || !GrabAbility)
	{
		return;
	}

	// Offer the grab ability to whoever is looking at us. Lyra's interaction ability grants/activates this
	// on the instigator when they choose the option -> GA_AFL_Grab fires with this actor as the target.
	FInteractionOption Option;
	Option.Text = InteractionText;
	Option.InteractionAbilityToGrant = GrabAbility;
	OptionBuilder.AddInteractionOption(Option);
}

FAFLGrabPolicy UAFLGrabbableComponent::GetGrabPolicy() const
{
	FAFLGrabPolicy Policy;
	Policy.HoldSocketName = HoldSocketName;
	Policy.HoldOffset = HoldOffset;
	Policy.ReleaseImpulseMagnitude = ReleaseImpulseMagnitude;
	Policy.ReleaseImpulseDirection = ReleaseImpulseDirection;
	Policy.ThrowImpulseMagnitude = ThrowImpulseMagnitude;
	Policy.bEnablePhysicsOnRelease = bEnablePhysicsOnRelease;
	Policy.CarryWeight = CarryWeight;
	Policy.ObjectAnimSet = ObjectAnimSet;
	return Policy;
}

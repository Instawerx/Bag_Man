// Copyright C12 AI Gaming. All Rights Reserved.

#include "Interaction/AFLGrabbableComponent.h"

#include "GameFramework/Actor.h"
#include "Interaction/InteractionOption.h"
#include "Interaction/InteractionQuery.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGrabbableComponent)

UAFLGrabbableComponent::UAFLGrabbableComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // pure marker + policy holder; no tick.

	// 2-client cycle 1: the component replicates so bHeld reaches every client (discovery gating + the
	// observer harness read the same truth everywhere). The owning ACTOR must also replicate for this to
	// flow -- grabbable BPs ship bReplicates + bReplicateMovement true (movable-asset rule).
	SetIsReplicatedByDefault(true);
}

void UAFLGrabbableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLGrabbableComponent, bHeld);
}

void UAFLGrabbableComponent::SetHeld(bool bInHeld)
{
	// Authority-only write: the predicted client grab calls through here too, but clients must converge on
	// the replicated value -- a server-rejected prediction that wrote local state would leave this grabbable
	// permanently "held" (= undiscoverable) on that client only.
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	bHeld = bInHeld;
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
	Policy.bDropOnDamage = bDropOnDamage;
	Policy.bEnablePhysicsOnRelease = bEnablePhysicsOnRelease;
	Policy.CarryWeight = CarryWeight;
	Policy.ObjectAnimSet = ObjectAnimSet;
	Policy.CarrierEffectClass = CarrierEffectClass;
	return Policy;
}

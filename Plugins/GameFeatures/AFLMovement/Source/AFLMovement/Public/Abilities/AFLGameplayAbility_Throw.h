// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_Throw.generated.h"

/**
 * UAFLGameplayAbility_Throw  (Object-Interaction, throw cycle)
 *
 * Aimed throw of the carried object. Activation REQUIRES State.Carrying (granted by the grab ability's
 * carry GE), which is what lets it share the weapon-fire input: while carrying, fire = throw; when not
 * carrying this ability simply cannot activate and the input belongs to the weapon again (the fire
 * ability gets State.Carrying as an ActivationBlockedTag on the data side -- the two tags are the whole
 * arbitration, no input rewiring).
 *
 * On activate: release the carried actor along the CONTROLLER AIM (full 3D -- pitch included) via
 * UAFLInteractionComponent::ReleaseActor(Throw, Dir), then CANCEL the active grab ability so the carry
 * state (hold pose, State.Carrying GE, ability lifetime) tears down through the grab's single EndAbility
 * funnel. Double-release is structurally impossible: this throw's release clears CarriedActor, so the
 * grab's own EndAbility ReleaseActor() call hits the !Target early-return (and the rifle was already
 * restored by the full release that ran here).
 *
 * Aim source = PlayerController->GetControlRotation().Vector() -- ability-side CLIENT aim; never
 * GetPlayerViewPoint (AFL lint rail). Server-side validation of the claimed throw ray (the hitscan
 * claimed-ray pattern) is a NAMED FUTURE item for the netcode pass.
 *
 * Not abstract: it has no per-asset class refs to author, so the AbilitySet grants this C++ class
 * directly (a BP child only becomes necessary if a cooldown/cost GE is added later).
 */
UCLASS()
class AFLMOVEMENT_API UAFLGameplayAbility_Throw : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_Throw(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};

// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Interaction/IInteractableTarget.h"   // Lyra interaction discovery interface (LyraGame module)

#include "AFLGrabbableComponent.generated.h"

class UGameplayAbility;
class UAFLObjectClassAnimSet;

/** Per-object carry weight -- placeholder hook for the future carry-state penalty cycle (Decision H). */
UENUM(BlueprintType)
enum class EAFLCarryWeight : uint8
{
	None  UMETA(DisplayName = "None"),
	Light UMETA(DisplayName = "Light"),
	Heavy UMETA(DisplayName = "Heavy"),
};

/**
 * FAFLGrabPolicy -- the per-object rules bundle the grab ability reads on grab. Populated from the
 * grabbable component's UPROPERTYs at grab time and handed to UAFLInteractionComponent::GrabActor so the
 * ability/interaction-component never reaches back into the world actor for policy mid-carry.
 *
 * This is the forward-compat seam: energy drops, dismembered heads, the stress-object, and mini-game props
 * each set different values (socket, release shape, weight) without any new code -- a designer edits the
 * grabbable component, or a future GrabPolicy subclass overrides the defaults per object class.
 */
USTRUCT(BlueprintType)
struct FAFLGrabPolicy
{
	GENERATED_BODY()

	/** Socket on the hero mesh the held actor snaps to. Default hand_r (Decision J: per-object, default single). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	FName HoldSocketName = TEXT("hand_r");

	/** Relative offset (cm, in the socket's space) the held actor sits at -- pushes it OUT of the hand bone so
	 *  it's visible in front of the palm, not buried inside the wrist mesh. Forward+up by default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	FVector HoldOffset = FVector(20.0f, 0.0f, 0.0f);

	/** Impulse magnitude applied on release -- the "slight ragdoll" brand-theme (modest, NOT a gravity-gun launch). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	float ReleaseImpulseMagnitude = 250.0f;

	/** Local-to-camera release direction (forward+up). Normalized at apply time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	FVector ReleaseImpulseDirection = FVector(1.0f, 0.0f, 0.35f);

	/** If true, physics is re-enabled on the held actor at release (the default; attach-only props set false). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	bool bEnablePhysicsOnRelease = true;

	/** Future carry-penalty cycle reads this (no penalty in the proof commit per Decision H). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	EAFLCarryWeight CarryWeight = EAFLCarryWeight::None;

	/** 4e: per-class anim dispatch. Shared anim set for this object's class (sibling to CarryWeight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	TSoftObjectPtr<UAFLObjectClassAnimSet> ObjectAnimSet;
};

/**
 * UAFLGrabbableComponent  (Object-Interaction substrate, world-actor side)
 *
 * Marks a world actor as physically carriable AND carries its per-object grab rules. Implements Lyra's
 * IInteractableTarget so the hero's interaction discovery (AbilityTask_WaitForInteractableTargets line-trace
 * + AbilityTask_GrantNearbyInteraction proximity) finds it and offers GA_AFL_Grab via FInteractionOption --
 * this is the "extend Lyra's discovery" half of the Hybrid (Decision F). The "build AFL's carry substrate"
 * half is UAFLInteractionComponent on the hero.
 *
 * Designer attaches this to any BP/actor (e.g. BP_AFL_TestGrabbable) and tunes the policy UPROPERTYs. The
 * forward-compat list (energy drops, heads, stress-object, mini-game props) are all just actors wearing this
 * component with different policy values -- zero new code per object kind.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLGrabbableComponent : public UActorComponent, public IInteractableTarget
{
	GENERATED_BODY()

public:
	UAFLGrabbableComponent();

	//~ IInteractableTarget
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder) override;
	//~ End IInteractableTarget

	/** Build the policy struct from this component's current UPROPERTY values (read by the grab ability). */
	UFUNCTION(BlueprintCallable, Category = "AFL|Interaction")
	FAFLGrabPolicy GetGrabPolicy() const;

	/** True while some interaction component is carrying this actor (set by the carrier; blocks double-grab). */
	UFUNCTION(BlueprintPure, Category = "AFL|Interaction")
	bool IsHeld() const { return bHeld; }

	/** Carrier sets/clears this on grab/release so a second instigator's discovery can reject an already-held actor. */
	void SetHeld(bool bInHeld) { bHeld = bInHeld; }

protected:
	/** The ability offered to the looking-at instigator. BP child sets GA_AFL_Grab_C. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction")
	TSubclassOf<UGameplayAbility> GrabAbility;

	/** Optional prompt text surfaced by the interaction widget ("Grab", "Pick Up", etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction")
	FText InteractionText;

	// --- Per-object policy (designer-editable; the forward-compat seam) ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	FName HoldSocketName = TEXT("hand_r");

	/** Offset from the socket so the held object sits in front of the palm, not inside the wrist (cm, socket space). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	FVector HoldOffset = FVector(20.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy", meta = (ClampMin = "0.0"))
	float ReleaseImpulseMagnitude = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	FVector ReleaseImpulseDirection = FVector(1.0f, 0.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	bool bEnablePhysicsOnRelease = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	EAFLCarryWeight CarryWeight = EAFLCarryWeight::None;

	/** 4e: shared per-class anim set this object dispatches (N heads -> one OCAS_Head). Forwarded into the policy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	TSoftObjectPtr<UAFLObjectClassAnimSet> ObjectAnimSet;

private:
	bool bHeld = false;
};

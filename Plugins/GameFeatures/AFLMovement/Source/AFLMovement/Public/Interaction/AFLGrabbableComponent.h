// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Interaction/IInteractableTarget.h"   // Lyra interaction discovery interface (LyraGame module)

#include "AFLGrabbableComponent.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UAFLObjectClassAnimSet;

/**
 * Fired (server-authority) the instant this grabbable is picked up, carrying the GRABBER pawn.
 * The generic "who grabbed me" seam: the world actor wearing this component binds in BeginPlay to
 * react to its own pickup without the grab path hard-knowing the actor's type. First consumer is
 * the dismembered-head loot-box (self-retrieve -> reattach vs enemy -> collect); energy drops /
 * stress-object / mini-game props can bind the same way. Grabber may be null if the carrier's owner
 * is not a pawn (defensive).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAFLOnGrabbedBy, AActor*, Grabber);

/** Per-object carry weight -- placeholder hook for the future carry-state penalty cycle (Decision H). */
UENUM(BlueprintType)
enum class EAFLCarryWeight : uint8
{
	None  UMETA(DisplayName = "None"),
	Light UMETA(DisplayName = "Light"),
	Heavy UMETA(DisplayName = "Heavy"),
};

/**
 * Routing: what picking this object up actually DOES (Loot-Carry Phase B). The grab ability forks on this
 * BEFORE the reach montage, so the two paths share discovery but split entirely after:
 *  - CarryObject (default) -- the proven hand-grab (attach to hand_r, carry-pose, holster). Map objects.
 *  - CollectLoot           -- route to the collect-channel (UAFLAG_CollectChannel) + the carried pool;
 *                             NOT hand-occupied. Loot caches. The grab ability sends the channel's trigger
 *                             event instead of attaching; the channel grants from this actor on complete.
 */
UENUM(BlueprintType)
enum class EAFLGrabKind : uint8
{
	CarryObject UMETA(DisplayName = "Carry Object (hand-grab)"),
	CollectLoot UMETA(DisplayName = "Collect Loot (channel -> pool)"),
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

	/** Impulse magnitude for an AIMED throw release (throw cycle; EAFLReleaseMode::Throw). Mass-scaled like
	 *  the drop impulse; the direction comes from the thrower's aim, not this policy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab", meta = (ClampMin = "0.0"))
	float ThrowImpulseMagnitude = 900.0f;

	/** Drop-on-damage: if true (default -- the stress-object/extraction pressure loop), the carrier taking ANY
	 *  confirmed damage force-drops this object. Per-object opt-out for things that should survive a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	bool bDropOnDamage = true;

	/** If true, physics is re-enabled on the held actor at release (the default; attach-only props set false). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	bool bEnablePhysicsOnRelease = true;

	/** Future carry-penalty cycle reads this (no penalty in the proof commit per Decision H). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	EAFLCarryWeight CarryWeight = EAFLCarryWeight::None;

	/** 4e: per-class anim dispatch. Shared anim set for this object's class (sibling to CarryWeight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	TSoftObjectPtr<UAFLObjectClassAnimSet> ObjectAnimSet;

	/** Per-object carrier effect (stress-object cycle; CarryWeight-sibling). Applied to the CARRIER's ASC
	 *  while this object is held (e.g. UGE_AFL_CarrierVulnerability -> State.Carrying.Vulnerable -> x1.3
	 *  damage taken via UAFLDamageExecCalc). Null (the default) = no per-object effect, behavior unchanged.
	 *  Removal is BY HANDLE in the grab's EndAbility, so any GE class works without a class-keyed lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Grab")
	TSoftClassPtr<UGameplayEffect> CarrierEffectClass;
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

	/** Routing kind (Loot-Carry Phase B): the grab ability forks on this before the reach montage. */
	UFUNCTION(BlueprintPure, Category = "AFL|Interaction")
	EAFLGrabKind GetGrabKind() const { return GrabKind; }

	/** Set the routing kind (loot caches mark themselves CollectLoot at spawn; default stays CarryObject). */
	void SetGrabKind(EAFLGrabKind InKind) { GrabKind = InKind; }

	/** True while some interaction component is carrying this actor (set by the carrier; blocks double-grab).
	 *  On clients this reads the REPLICATED value (2-client cycle 1) -- discovery gating is correct remotely. */
	UFUNCTION(BlueprintPure, Category = "AFL|Interaction")
	bool IsHeld() const { return bHeld; }

	/** Carrier sets/clears this on grab/release so a second instigator's discovery can reject an already-held
	 *  actor. AUTHORITY-ONLY write (2-client cycle 1): the predicted client-side grab path calls this too, but
	 *  only the server's write lands -- a rejected prediction can no longer poison the client's local state
	 *  (the stale-bHeld risk); clients converge on the replicated truth. */
	void SetHeld(bool bInHeld);

	/** Broadcasts OnGrabbedBy with the grabber. Called by UAFLInteractionComponent::GrabActor right after
	 *  SetHeld(true), server-side. Kept as a tiny forwarder so the broadcast site stays inside the component. */
	void NotifyGrabbedBy(AActor* Grabber);

	/** "Who grabbed me" -- bound by the world actor (e.g. the head loot-box) to react to its own pickup. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Interaction")
	FAFLOnGrabbedBy OnGrabbedBy;

	//~ UActorComponent
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent

protected:
	/** The ability offered to the looking-at instigator. BP child sets GA_AFL_Grab_C. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction")
	TSubclassOf<UGameplayAbility> GrabAbility;

	/** Optional prompt text surfaced by the interaction widget ("Grab", "Pick Up", etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction")
	FText InteractionText;

	// --- Per-object policy (designer-editable; the forward-compat seam) ---

	/** Routing: hand-grab (default, map objects) vs collect-channel-to-pool (loot caches). Loot-Carry Phase B. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	EAFLGrabKind GrabKind = EAFLGrabKind::CarryObject;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	FName HoldSocketName = TEXT("hand_r");

	/** Offset from the socket so the held object sits in front of the palm, not inside the wrist (cm, socket space). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	FVector HoldOffset = FVector(20.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy", meta = (ClampMin = "0.0"))
	float ReleaseImpulseMagnitude = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	FVector ReleaseImpulseDirection = FVector(1.0f, 0.0f, 0.35f);

	/** Aimed-throw impulse magnitude (throw cycle). Forwarded into the policy like its siblings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy", meta = (ClampMin = "0.0"))
	float ThrowImpulseMagnitude = 900.0f;

	/** Drop-on-damage opt-out (default ON). Forwarded into the policy like its siblings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	bool bDropOnDamage = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	bool bEnablePhysicsOnRelease = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	EAFLCarryWeight CarryWeight = EAFLCarryWeight::None;

	/** 4e: shared per-class anim set this object dispatches (N heads -> one OCAS_Head). Forwarded into the policy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	TSoftObjectPtr<UAFLObjectClassAnimSet> ObjectAnimSet;

	/** Per-object carrier effect applied to the carrier while held (stress-object cycle). Forwarded into the policy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction|Policy")
	TSoftClassPtr<UGameplayEffect> CarrierEffectClass;

private:
	/** Replicated held flag (2-client cycle 1, the module's first replicated property): the server's
	 *  GrabActor/ReleaseActor own the writes; every client's discovery + the grab ability's validation read
	 *  the same truth. Plain RepNotify-less replication -- consumers poll IsHeld(), nothing reacts to edges. */
	UPROPERTY(Replicated)
	bool bHeld = false;
};

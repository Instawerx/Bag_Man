// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "HUD/AFLHitConfirmInfo.h"

#include "AFLHitConfirmComponent.generated.h"

class UCameraShakeBase;
struct FAFLHitConfirmMessage;
struct FGameplayTag;


/**
 * UAFLHitConfirmComponent
 *
 * Local-client hit-confirmation feedback for the FIRING player. Lives on the
 * local player controller (added via the AFLCombat hero-components action set
 * or attached to ALyraPlayerController by experience config). Listens to the
 * Event.Damage.Confirmed channel on UGameplayMessageSubsystem; that channel
 * is broadcast by UAFLDamageExecCalc on the server when EffectiveDamage > 0,
 * and is replicated to the firing client by the verb-message replication
 * graph that Lyra already wires for damage-tag broadcasts.
 *
 * On confirm:
 *   1. Triggers ClientStartCameraShake with CameraShakeClass (default
 *      UAFLCameraShake_HitConfirm — designers reskin via a BP child).
 *   2. Broadcasts OnHitConfirmed(FAFLHitConfirmInfo) for HUD widgets
 *      (WBP_AFL_HitMarker) and AFL-0204b damage-number popups.
 *
 * Strictly client-feedback. Does not participate in authoritative damage flow.
 * AFL-0215 lint rule: no SetHealth / GiveAbility paths leave this component.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAFLOnHitConfirmed, const FAFLHitConfirmInfo&, Info);


UCLASS(ClassGroup=(AFL), meta=(BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLHitConfirmComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UAFLHitConfirmComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Camera shake class triggered on confirm. BP children may override per-weapon-feel in S5. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|HitConfirm")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	/**
	 * Bone-name fragments that flag a confirm as a headshot. Substring match on
	 * the FHitResult.BoneName (case-insensitive) — placeholder until AFL-0411
	 * publishes the per-pawn body-zone enum and we can drop the heuristic.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|HitConfirm")
	TArray<FName> HeadshotBoneFragments;

	/** Listeners receive a copy of the FAFLHitConfirmInfo payload on every confirm. */
	UPROPERTY(BlueprintAssignable, Category="AFL|HitConfirm")
	FAFLOnHitConfirmed OnHitConfirmed;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Listener token from UGameplayMessageSubsystem, retained for symmetric unregister in EndPlay. */
	FGameplayMessageListenerHandle ListenerHandle;

	/** Bound to Event.Damage.Confirmed via UGameplayMessageSubsystem::RegisterListener. */
	void OnDamageConfirmed(FGameplayTag Channel, const FAFLHitConfirmMessage& Payload);

	/** Returns the player controller owning this component (component lives on PC or its pawn). */
	class APlayerController* GetOwningPlayerController() const;

	/** Match BoneName against HeadshotBoneFragments (case-insensitive substring). */
	bool IsHeadshotBone(FName BoneName) const;
};

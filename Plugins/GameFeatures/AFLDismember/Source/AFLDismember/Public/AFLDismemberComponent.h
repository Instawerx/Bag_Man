// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "AFLDismemberComponent.generated.h"

struct FAFLOverkillMessage;
struct FGameplayTag;

/**
 * Victim-side dismemberment listener. Subscribes to Event.Damage.Overkill.AFL
 * (broadcast server-side by UAFLDamageExecCalc, carrying BoneName) and, when
 * the overkill victim is THIS component's owner and the killing blow hit the
 * head, triggers head detachment.
 *
 * S4-04 SCOPE: detection + logging only. The actual detach (head physics
 * actor + bone hide) is S4-05.
 *
 * SHAPE NOTE -- deliberately NOT a copy of UAFLHitConfirmComponent's filter:
 * HitConfirm is client-feedback (gates on IsLocalController, filters "we
 * instigated"). This is the OPPOSITE -- the overkill broadcast is server-side
 * and dismemberment is authoritative gameplay on the VICTIM. So: register only
 * on the server (HasAuthority), NO local-controller gate, filter on
 * Payload.Target == GetOwner() (am I the one who died?).
 */
UCLASS(ClassGroup=(AFL), meta=(BlueprintSpawnableComponent))
class AFLDISMEMBER_API UAFLDismemberComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLDismemberComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnOverkill(FGameplayTag Channel, const FAFLOverkillMessage& Payload);

	FGameplayMessageListenerHandle ListenerHandle;
};

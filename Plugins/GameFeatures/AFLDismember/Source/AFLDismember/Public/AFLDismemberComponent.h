// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "AFLDismemberComponent.generated.h"

class UAFLDismemberZoneSet;
struct FAFLDismemberZone;
struct FAFLOverkillMessage;
struct FGameplayTag;

/**
 * Victim-side dismemberment listener. Subscribes to Event.Damage.Overkill.AFL
 * (broadcast server-side by UAFLDamageExecCalc, carrying BoneName) and, when the
 * overkill victim is THIS component's owner, resolves the killing-blow bone to a
 * zone row in the ZoneSet and severs that zone.
 *
 * S4-INC1: DATA-DRIVEN. The former hard-coded head path (BoneName=="head", a
 * TSubclassOf<AAFLDismemberedHead>, literal +-100/+500 impulse, the HeadPop cue) is
 * generalized into the ZoneSet table: FindZoneForBone -> SeverZone(row), where the row
 * carries the severed bone, the (soft) prop class, the impulse range, the (soft) cue,
 * and the (soft) consequence GE. The head is just the Head row -- the proven head
 * behavior is preserved by authoring that row identically (PHASE B).
 *
 * SHAPE NOTE -- deliberately NOT a copy of UAFLHitConfirmComponent's filter:
 * HitConfirm is client-feedback (gates on IsLocalController, filters "we instigated").
 * This is the OPPOSITE -- the overkill broadcast is server-side and dismemberment is
 * authoritative gameplay on the VICTIM. So: register only on the server (HasAuthority),
 * NO local-controller gate, filter on Payload.Target == GetOwner() (am I the one who died?).
 *
 * AAA: the ZoneSet + every per-zone asset (prop class, consequence GE, spark FX) is a
 * SOFT reference. The component async-loads the ZoneSet, then async-loads the matched
 * row's prop/GE before spawning/applying -- no hard refs baked into the component, no
 * hard cast to any specific pawn/BP (the zone is resolved by bone -> data).
 */
UCLASS(ClassGroup=(AFL), meta=(BlueprintSpawnableComponent))
class AFLDISMEMBER_API UAFLDismemberComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLDismemberComponent();

	/** The data-driven zone table (soft, async-loaded at sever time). Authored in PHASE B. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Dismember")
	TSoftObjectPtr<UAFLDismemberZoneSet> ZoneSet;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnOverkill(FGameplayTag Channel, const FAFLOverkillMessage& Payload);

	/** Async-load the ZoneSet, resolve HitBone -> row, then SeverZone(row). Server-only. */
	void ResolveAndSever(FName HitBone);

	/**
	 * Sever one zone (server-authority). Generalizes the proven head path:
	 *   1. HideBoneByName(Row.SeveredBone, PBO_Term) on the owner's skeletal mesh
	 *   2. spawn Row.PropClass (async-loaded) at the severed-bone world transform
	 *   3. ApplyPopImpulse (randomized within Row.ImpulseXYRange + ImpulseZ)
	 *   4. fire Row.CueTag (cosmetic, replicated via the victim ASC) if set
	 *   5. apply Row.ConsequenceGE (async-loaded) to the victim ASC if set
	 * Row is copied (not pointer-held) because the async continuations outlive the
	 * FindZoneForBone scan's array-pointer lifetime.
	 */
	void SeverZone(const FAFLDismemberZone& Row);

	FGameplayMessageListenerHandle ListenerHandle;
};

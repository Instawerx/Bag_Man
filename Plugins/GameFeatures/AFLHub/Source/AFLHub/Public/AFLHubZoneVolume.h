// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "AFLHubZoneVolume.generated.h"

class UBoxComponent;
class UGameplayEffect;

/**
 * AAFLHubZoneVolume  (AFL-3015 / H1.5 -- flow N4a-N4h zone tagging)
 *
 * A replicated tag dispenser, nothing more -- conformed to AAFLExtractionZone (the proven
 * EXTRACTION-CYCLE1 overlap/authority shape: server-side overlap applies/removes an infinite GE,
 * tracked BY HANDLE per pawn with weak keys, EndPlay sweep). Differences from the sibling, each
 * deliberate:
 *  - Always active: a hub zone has no window phase, so the state machine and phase observers are
 *    gone rather than carried dormant.
 *  - ONE shared GE asset (GE_AFL_Hub_Zone) for all eleven zones: the per-zone tag rides the SPEC
 *    via DynamicGrantedTags, so adding a zone is a placed volume + a tag, never a new GE.
 *  - Box, not sphere: zones wrap buildings (HUB-ZONE-ASSIGNMENT rows), and the designer scales the
 *    box on the instance.
 *
 * It NEVER touches Hub.Restriction.NoFire -- fire authorisation is by EXPERIENCE (SSOT s2.3);
 * the volume's only output is Hub.Zone.<Name> on the pawn's ASC, which the net profile component,
 * zone prompts and chat scoping consume.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLHUB_API AAFLHubZoneVolume : public AActor
{
	GENERATED_BODY()

public:
	AAFLHubZoneVolume();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** The zone box (root). Extent is the designer knob on the placed instance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	TObjectPtr<UBoxComponent> ZoneBox;

	/** Which Hub.Zone.* this volume grants. The ONLY per-zone datum. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Hub", meta = (Categories = "Hub.Zone"))
	FGameplayTag ZoneTag;

	/** The shared infinite tag-carrier GE (GE_AFL_Hub_Zone). The zone tag is added to the spec's
	 *  DynamicGrantedTags, so one asset serves every zone. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Hub")
	TSubclassOf<UGameplayEffect> ZoneEffectClass;

private:
	/** Apply the zone GE to a pawn (authority only; no-op when already tracked -- the idempotence
	 *  the AC demands across possession double-fire and respawn inside the volume). */
	void TryApplyTo(AActor* PawnActor);

	/** Remove + forget every live handle (EndPlay sweep -- the sibling's RemoveAllZoneEffects). */
	void RemoveAllZoneEffects();

	/** Live per-pawn handles (carrier-GE handle-tracking precedent; weak keys sweep dead pawns). */
	TMap<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle> ZoneEffectHandles;
};

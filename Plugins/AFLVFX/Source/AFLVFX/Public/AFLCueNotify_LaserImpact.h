// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "AFLCueNotify_LaserImpact.generated.h"

class UNiagaraSystem;

/**
 * Burst cue for a laser impact (GameplayCue.Weapon.Laser.Impact).
 *
 * One-shot: on execute, spawn the impact spark Niagara at Params.Location oriented to
 * Params.Normal, then auto-destroy. The spark system comes from the weapon's
 * IAFLLaserVisualProvider (its muzzle/impact system slot) read off
 * FGameplayCueParameters::SourceObject -- look is data, not code.
 *
 * No tick: everything is read from cue params / SourceObject at execute time. This is
 * the hitscan/per-hit spark sibling of the looping AAFLCueNotify_LaserBeam.
 *
 * DISCOVERY: this C++ class is a PARENT only. To fire, a tagged GCN_AFL_*.uasset
 * parented to it must live under a scanned GameplayCueNotifyPaths folder
 * (/Game/GameplayCues/) -- Lyra's GameplayCueManager is asset-scan + path-based, not
 * C++-class-scanned. (See afl-laser-beam-system integration-architecture.md.)
 */
UCLASS()
class AFLVFX_API AAFLCueNotify_LaserImpact : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AAFLCueNotify_LaserImpact();

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	/**
	 * Fallback spark system if the weapon's provider supplies none (or SourceObject
	 * isn't a provider). Optional; leave null to require the provider to supply it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	TObjectPtr<UNiagaraSystem> DefaultImpactSystem = nullptr;

	/** Niagara user-param name for the beam tint, matched to the marketplace systems (User.Color). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	FName ColorParam = TEXT("Color");
};

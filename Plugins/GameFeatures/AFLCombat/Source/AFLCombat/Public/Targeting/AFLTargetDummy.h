// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacterWithAbilities.h"

#include "AFLTargetDummy.generated.h"

class UAFLDeathComponent;
class UAFLAttributeSet_Combat;


/**
 * AAFLTargetDummy
 *
 * The first CONSUMER of the AFL death system (UAFLDeathComponent), and the standing combat
 * target every future test runs against. It is NOT where death logic lives -- it just grants
 * the reusable death component and reacts to damage. The player and every enemy reuse the same
 * UAFLDeathComponent; this dummy proves it works end-to-end before the player inherits it.
 *
 * Base = ALyraCharacterWithAbilities: a real Lyra pawn that owns its OWN typed
 * ULyraAbilitySystemComponent in its ctor, so AFLCombat's GameFeatureAction_AddAbilities
 * CastChecked<ULyraAbilitySystemComponent> succeeds (BM-DEBT-004-safe -- the unpossessed-
 * ALyraCharacter crash does not fire). It also inherits ALyraCharacter's ULyraHealthComponent,
 * whose replicated StartDeath/FinishDeath the UAFLDeathComponent drives from the AFL signal.
 *
 * The AFL combat attribute set (UAFLAttributeSet_Combat, Health=100 + GE_AFL_Combat_InitData)
 * is granted by the experience's GameFeatureAction_AddAbilities entry keyed on this class
 * (the proven BM-0102 1a path) -- the same DA_AFL_Combat_AbilitySet the player gets. Mesh,
 * placement, and the hit-react VISUALS are content/BP on top of this C++ base.
 */
UCLASS()
class AFLCOMBAT_API AAFLTargetDummy : public ALyraCharacterWithAbilities
{
	GENERATED_BODY()

public:
	AAFLTargetDummy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;

	/**
	 * Bound to UAFLAttributeSet_Combat::OnHealthChanged for the per-hit react. The visible react
	 * itself (flash MID / hit montage) is a BlueprintImplementableEvent so the look is authored
	 * in the BP child (operator-tunable), keeping this C++ base structural-only.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Target")
	void OnDamageReact(float Magnitude);

	/** The reusable AFL death driver (Health<=0 -> Lyra's replicated death sequence). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Target")
	TObjectPtr<UAFLDeathComponent> DeathComponent;

private:
	void HandleHealthChanged(AActor* Instigator, AActor* Causer, float Magnitude);

	const UAFLAttributeSet_Combat* CombatSet = nullptr;
	FDelegateHandle HealthChangedHandle;
};

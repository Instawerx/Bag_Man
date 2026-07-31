// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_Roll.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;

/**
 * UAFLGameplayAbility_Roll  (Movement Overhaul — Phase 2: directional dodge-roll)
 *
 * Root-motion dodge-roll. Resolves a 4-way direction from the pawn's movement input RELATIVE to its facing
 * (the Dash direction-resolve), then plays the matching section of the roll montage — so the character rolls
 * forward/back/left/right while KEEPING its facing (a combat dodge, not a turn-and-run). Root motion carries
 * the roll; no CMC component. A short State.Movement.Rolling GE gates conflicts + drives AI/anim.
 *
 * I-frames are RESERVED but OFF by default (mirrors the Dash i-frame rule — reserved in AFLCoreTags.ini, not
 * enabled until design signs off): set bGrantIFrames + IFrameEffectClass to grant State.Invulnerable for a
 * sub-window of the roll.
 *
 * Abstract; the GA_AFL_Roll BP child sets the GE + montage. InstancedPerActor, LocalPredicted, OnInputTriggered.
 */
UCLASS(Abstract)
class AFLMOVEMENT_API UAFLGameplayAbility_Roll : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_Roll(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** Duration GE applied on activation — grants State.Movement.Rolling (mirror GE_AFL_Slide_Active). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Roll")
	TSubclassOf<UGameplayEffect> RollActiveEffectClass;

	/** Root-motion roll montage. BP child sets it. Expects 4 directional sections (see the section names). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Roll")
	TObjectPtr<UAnimMontage> RollMontage;

	/** Montage sections for each roll direction (root motion authored per-direction, facing preserved). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Roll")
	FName ForwardSection = TEXT("Forward");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Roll")
	FName BackwardSection = TEXT("Backward");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Roll")
	FName LeftSection = TEXT("Left");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Roll")
	FName RightSection = TEXT("Right");

	/** Movement-input magnitude below which we roll Forward (no clear directional intent). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Roll")
	float DirectionDeadzone = 0.25f;

	/** RESERVED, OFF by default: grant State.Invulnerable via IFrameEffectClass for a sub-window of the roll. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Roll")
	bool bGrantIFrames = false;

	/** Duration GE granting State.Invulnerable (its own timing window). Only applied when bGrantIFrames. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Roll")
	TSubclassOf<UGameplayEffect> IFrameEffectClass;

private:
	UFUNCTION()
	void OnMontageCompleted();
	UFUNCTION()
	void OnMontageInterruptedOrCancelled();

	void ExitRoll(const TCHAR* Reason, bool bCancelled);

	/** Resolve the roll direction from movement input relative to facing -> the matching montage section. */
	FName ResolveRollSection() const;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	bool bExiting = false;
};

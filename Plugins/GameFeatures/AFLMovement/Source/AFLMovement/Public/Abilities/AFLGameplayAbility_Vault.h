// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_Vault.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;

/**
 * UAFLGameplayAbility_Vault  (Movement Overhaul — Phase 2: vault-over)
 *
 * The MOTION half of vault, mirroring GA_AFL_Climb's Motion-Warp + montage pattern. On activation it probes
 * for a waist/chest-height obstacle ahead (forward + top-edge + landing traces); if one is vaultable it warps
 * a root-motion vault montage onto the obstacle top (WarpToVaultTop) and the far-side landing (WarpToVaultLand),
 * so a fixed-distance vault clip fits obstacles of varying depth/height without clipping. No CMC-state
 * component is needed — root motion carries the vault; a short State.Movement.Vaulting GE just blocks
 * conflicting moves and drives AI/anim. Abstract; the GA_AFL_Vault BP child sets the GE + montage.
 *
 * Native defaults: InstancedPerActor, LocalPredicted, OnInputTriggered (press-to-vault when facing an
 * obstacle; a no-op otherwise).
 *
 * v1 = vault-OVER only. The context slide-under handoff (low ceiling over the obstacle -> activate slide)
 * and auto-vault-on-jump are Phase-4 refinements.
 */
UCLASS(Abstract)
class AFLMOVEMENT_API UAFLGameplayAbility_Vault : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_Vault(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

	/** Duration GE applied on activation — grants State.Movement.Vaulting (mirror GE_AFL_Climb_Active). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Vault")
	TSubclassOf<UGameplayEffect> VaultActiveEffectClass;

	/** Root-motion vault montage. BP child sets it. Author with AnimNotifyState_MotionWarping windows
	 *  targeting WarpToVaultTop (hand-plant on the top edge) and WarpToVaultLand (far-side landing). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Vault")
	TObjectPtr<UAnimMontage> VaultMontage;

	/** Forward trace length (cm) to find a vaultable obstacle in front. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Vault")
	float ForwardTraceDistance = 100.0f;

	/** Obstacle top edge must sit between these heights (cm, above the pawn's feet) to be vault-OVER-able. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Vault")
	float MinObstacleHeight = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Vault")
	float MaxObstacleHeight = 130.0f;

	/** How far (cm) beyond the top edge we probe for a clear landing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Vault")
	float LandingProbeDistance = 120.0f;

	/** Warp target names — must match the AnimNotifyState_MotionWarping windows on VaultMontage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Vault")
	FName VaultTopWarpTargetName = TEXT("WarpToVaultTop");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Vault")
	FName VaultLandWarpTargetName = TEXT("WarpToVaultLand");

private:
	UFUNCTION()
	void OnMontageCompleted();
	UFUNCTION()
	void OnMontageInterruptedOrCancelled();

	/** Single-exit-wins end helper. */
	void ExitVault(const TCHAR* Reason, bool bCancelled);

	/** Probe for a vaultable obstacle. Outputs the top-edge hand-plant point, the far-side landing point, and
	 *  the facing (into the obstacle). Returns false if nothing vaultable is in front. */
	bool DetectVault(FVector& OutTopEdge, FVector& OutLanding, FRotator& OutFacing) const;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	bool bWarpApplied = false;
	bool bExiting = false;
};

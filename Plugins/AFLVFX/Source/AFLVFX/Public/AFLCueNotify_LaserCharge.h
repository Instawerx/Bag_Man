// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "AFLCueNotify_LaserCharge.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UCameraShakeBase;
class APawn;

/**
 * Looping charge build-up cue (GameplayCue.Weapon.Laser.Charge). AddGameplayCue'd by UAFLAG_Laser_Charge on
 * activate, RemoveGameplayCue'd on release -- OnRemove is the single cleanup site (looping-cue doctrine).
 *
 * THIN cosmetic (mirrors AAFLCueNotify_LaserBeam's discipline -- does NO gameplay): spawns a charge-orb
 * Niagara attached to the firing pawn's weapon socket, ramps its "Charge" user param 0..1 over
 * ChargeRampTime (a self-ramp -- the build-up is cosmetic, decoupled from the ability's exact charge
 * timing), tints it from the weapon's IAFLLaserVisualProvider colour, and (local player only) plays a
 * ramping camera shake. The ability owns charge/fire/damage; this only shows the wind-up.
 *
 * The orb system + shake are wired on the GCN_AFL_Laser_Charge child asset (adopt an existing orb from the
 * AFLVFXLibrary, e.g. NS_AFL_OrbLaser_Center_*). Pooled by the GC manager -> OnRemove nulls captured ptrs.
 */
UCLASS()
class AFLVFX_API AAFLCueNotify_LaserCharge : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AAFLCueNotify_LaserCharge();

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** The charge-orb Niagara (wired on the GCN child). Adopt one from the AFLVFXLibrary (NS_AFL_OrbLaser_Center_*). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	TObjectPtr<UNiagaraSystem> ChargeSystem;

	/** Niagara user param driven 0..1 by the charge ramp each tick (the build-up intensity). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	FName ChargeParam = TEXT("Charge");

	/** Niagara colour param (tint from the provider, matching the beam cue's User.Color contract). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	FName ColorParam = TEXT("Color");

	/** Socket on the firing pawn's mesh to attach the orb -- the weapon grip (matches the WID attach socket). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	FName AttachSocket = TEXT("weapon_r");

	/** Seconds the cosmetic build-up ramps over (mirror the ability's MaxChargeTime; cosmetic-only). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float ChargeRampTime = 1.5f;

	/** Optional local-player camera shake while charging (started on active, stopped on remove). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	TSubclassOf<UCameraShakeBase> ChargeCameraShake;

	/** The spawned orb component. Nulled in OnRemove (pool-safe). */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ChargeNC = nullptr;

	/** The firing pawn (weak -- may die mid-charge). */
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> FiringPawn;

	/** The weapon (IAFLLaserVisualProvider) the tint reads from. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> VisualProvider;

	/** Cosmetic ramp accumulator (seconds since OnActive). */
	float ElapsedCharge = 0.0f;

	/** Whether the local camera shake was started (so OnRemove stops exactly once). */
	bool bShakeStarted = false;
};

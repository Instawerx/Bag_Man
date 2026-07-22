// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"

#include "AFLEMPDevice.generated.h"

class UBoxComponent;
class USkeletalMeshComponent;
class ULyraAbilitySystemComponent;
class ULyraHealthSet;
class UAFLAttributeSet_Combat;
class UGameplayEffect;
struct FOnAttributeChangeData;

/**
 * AAFLEMPDevice
 *
 * The deployed EMP device the EMP puck spawns on landing (map-play power weapon; sibling of
 * AAFLDeployableBarrier). Like the barrier it is a LEAN replicated AActor -- deliberately NOT a pawn
 * (a pawn is counted by round/team/alive logic and corrupts the match) -- that self-grants the exact
 * ULyraAbilitySystemComponent + ULyraHealthSet + UAFLAttributeSet_Combat stack B_AFL_TargetDummy
 * proved takes AFL weapon damage, so it is DESTRUCTIBLE. Where the barrier BLOCKS, the device ARMS
 * then PULSES ONCE:
 *
 *   land -> ARM (ArmTime, ~1.5s: the BP ramps the reactor emissive + a charge audio = the TELEGRAPH;
 *   the device is DESTROYABLE this whole window = the COUNTERPLAY) -> single PULSE (sphere-overlap
 *   PulseRadius; every ENEMY pawn caught gets DisableGE for DisableDuration, team-filtered so
 *   friendlies are never touched) -> CONSUMED (one pulse, then despawn).
 *
 * DISABLE CONTRACT: DisableGE grants State.Weapon.Disabled, which lives in UAFLAG_Laser_Base's
 * ActivationBlockedTags -- and the ENTIRE AFL weapon roster descends from that base, so the tag
 * blocks every AFL weapon/throw ability at once, yet NOTHING else (movement/dash are separate
 * abilities, so a disabled player is disarmed but can still FLEE). This is the SMG overheat-lockout
 * shape (a GE-granted State tag in ActivationBlockedTags) turned on ENEMIES instead of self.
 *
 * Shot down before the pulse -> Health<=0 -> fizzle, NO pulse (the counterplay pays off). Delivery is
 * the unchanged UAFLAG_Deployable_Base throw (DeployableClass = this); the device owns the deployed
 * behaviour, exactly as the barrier owns the wall.
 */
UCLASS()
class AFLCOMBAT_API AAFLEMPDevice : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AAFLEMPDevice();

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End

protected:

	virtual void BeginPlay() override;

	/**
	 * Root: the shoot/damage collision volume. A primitive BOX, NOT the skeletal mesh -- the harvested
	 * SK_Tripmine has no guaranteed simple collision (skeletal collision needs a physics asset), so a
	 * box gives reliable "shoot it during arm" hittability independent of the mesh. BlockAll (default)
	 * is the barrier's proven AFL-weapon hittability.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AFL|EMP")
	TObjectPtr<UBoxComponent> CollisionBox;

	/** The device mesh (SK_Tripmine reskin). Visual only -- carries the reactor MI whose emissive the
	 *  BP ramps during arm (the telegraph). Attached under the collision box; the box is the hit target. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AFL|EMP")
	TObjectPtr<USkeletalMeshComponent> DeviceMesh;

	/** Self-owned ASC so AFL weapon-damage GEs have a valid target (the barrier's destructible stack). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AFL|EMP")
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;

	/** The Lyra health the ExecCalc outputs damage to (Health/MaxHealth/Damage meta). */
	UPROPERTY()
	TObjectPtr<ULyraHealthSet> HealthSet;

	/** The AFL combat set the ExecCalc captures (Armor/Shield/zones -> all 0 -> damage passes to Health). */
	UPROPERTY()
	TObjectPtr<UAFLAttributeSet_Combat> CombatSet;

	// ---- DESIGN KNOBS (operator-tuned; rulings baked as defaults) ----

	/** Seconds from land to the pulse. DESTROYABLE this whole window (counterplay); the BP ramps the
	 *  reactor emissive over exactly this long (the telegraph). Ruling: 1.5s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|EMP|Tuning", meta=(ClampMin="0.1"))
	float ArmTime = 1.5f;

	/** Pulse sphere radius (cm). Every ENEMY pawn inside at the pulse instant is disabled. Ruling: 550. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|EMP|Tuning", meta=(ClampMin="1.0"))
	float PulseRadius = 550.0f;

	/** How long caught enemies keep State.Weapon.Disabled (s) -- the block's "Duration 2.5". Passed to
	 *  DisableGE via SetByCaller (Data.Combat.EMPDuration) so THIS knob is the single source of truth
	 *  (author GE_AFL_EMP_Disable's Duration Magnitude as SetByCaller of that tag). Ruling: 2.5s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|EMP|Tuning", meta=(ClampMin="0.1"))
	float DisableDuration = 2.5f;

	/** Device hit points. Health + MaxHealth seed to this on BeginPlay; <=0 during arm -> fizzle (no pulse). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|EMP|Tuning", meta=(ClampMin="1.0"))
	float MaxHP = 120.0f;

	/** Collision profile the box uses. Default BlockAll = shootable (the barrier's proven hittability). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|EMP|Tuning")
	FName BlockProfileName = FName(TEXT("BlockAll"));

	/** The GE applied to every caught enemy on pulse -- grants State.Weapon.Disabled (block-fire) for
	 *  DisableDuration. Set to GE_AFL_EMP_Disable in the BP child. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|EMP|Tuning")
	TSubclassOf<UGameplayEffect> DisableGE;

	// ---- BP FX HOOKS (content-authored; the C++ stays structural) ----

	/**
	 * Arm START -- fired on EVERY machine in BeginPlay (server + each client), so the BP telegraph runs
	 * everywhere without an authority gate. The BP ramps the reactor emissive (a MID scalar) + a charge
	 * audio loop over ArmTime: the telegraph AND the reactor read. When the actor despawns its
	 * components (audio) are torn down, so the loop stops for free.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="AFL|EMP")
	void OnArmStart();

	/**
	 * The PULSE moment -- fired authority-side once the disable is already applied. The BP triggers a
	 * REPLICATED EMP-burst GameplayCue (ring Niagara + audio + shake) so all clients see the pop (the
	 * barrier's server-fired-cue-replicates pattern; a bare BP event here would run server-only).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="AFL|EMP")
	void OnPulse();

	/**
	 * Shot down BEFORE the pulse -- fired authority-side. The BP triggers a replicated fizzle/short-out
	 * cue. No enemy is disabled (the counterplay paid off).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="AFL|EMP")
	void OnDeviceFizzled();

private:

	/** Bound to the ASC Health delegate (authority-only); Health<=0 during arm -> fizzle (no pulse). */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** Arm timer callback (authority) -> Pulse() then Consume(true). */
	void HandleArmComplete();

	/** Sphere-overlap PulseRadius; apply DisableGE to every ENEMY ASC (team-filtered off the thrower). */
	void Pulse();

	/** One-shot end: guard, clear timers, stop collision, fizzle cue if !bPulsed, short-lifespan despawn. */
	void Consume(bool bPulsed);

	bool bConsumed = false;
	FTimerHandle ArmTimerHandle;
};

// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"

#include "AFLLagTestDummy.generated.h"

class ULyraAbilitySystemComponent;
class UAFLAttributeSet_Combat;
class UAFLPawnHitboxHistoryComponent;
class USkeletalMeshComponent;

/**
 * AAFLLagTestDummy
 *
 * BM-0105b lag-compensation test sink: a self-contained damageable target that
 * (1) has a SKELETAL mesh (so UAFLPawnHitboxHistoryComponent has bones to
 * snapshot) and (2) MOVES over time (so the rewind window samples a meaningfully
 * different pose than "now"). The static AAFLDamageTarget cube has neither — its
 * pose at "now" and "100ms ago" are identical, so it can't exercise lag-comp.
 *
 * ASC self-containment mirrors AAFLDamageTarget exactly (which mirrors
 * ALyraCharacterWithAbilities): ctor-created ULyraAbilitySystemComponent +
 * UAFLAttributeSet_Combat, InitAbilityActorInfo(self,self) in
 * PostInitializeComponents. Plain AActor (NOT ALyraCharacter) to sidestep
 * BM-DEBT-004's CastChecked crash on unpossessed Lyra pawns.
 *
 * The UAFLPawnHitboxHistoryComponent is carried by THIS actor in the ctor, so it
 * self-registers with UAFLLagCompensationWorldSubsystem on BeginPlay (server-only)
 * and pushes a 60Hz ring of the 8 default tracked bones (which match SKM_Manny).
 * That makes the dummy a registered rewind target without needing a GameFeature
 * AddComponents grant — the player-pawn grant is deferred to BM-0105c.
 *
 * Motion: sinusoidal lateral (Y-axis) sweep around the BeginPlay origin, so a
 * forward-facing shooter sees the dummy cross left-to-right. Freq/Amplitude are
 * UPROPERTY so BM-0105c can tune the speed for the latency-cohort test.
 */
UCLASS(Blueprintable)
class AFLCOMBAT_API AAFLLagTestDummy : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAFLLagTestDummy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	//~End of AActor interface

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

protected:
	/** Radians/sec of the sinusoidal sweep. Default ~3.14 = one full cycle per ~2s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|LagTest", meta = (AllowPrivateAccess = true))
	float SweepFrequency = 3.14159f;

	/**
	 * Peak lateral offset in cm from the spawn origin. 400cm so the BM-0105c
	 * compensation proof discriminates decisively: at 400cm/3.14159rad-s the
	 * dummy moves ~235cm during a 200ms rewind window (peak velocity ~1256cm/s),
	 * far wider than the ~115cm padded hit box — so accept-at-past vs
	 * reject-at-current is robust to imperfect shot timing. This is test-fixture
	 * tuning; the lag-comp system under test is unchanged.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|LagTest", meta = (AllowPrivateAccess = true))
	float SweepAmplitude = 400.0f;

private:
	/** Skeletal mesh root. SKM_Manny so the history component's 8 default tracked bones resolve. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|LagTest", meta = (AllowPrivateAccess = true))
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** Self-contained Lyra ASC. SetIsReplicated + Mixed mode mirror AAFLDamageTarget / ALyraCharacterWithAbilities. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|LagTest", meta = (AllowPrivateAccess = true))
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;

	/** 60Hz hitbox ring publisher. Self-registers with the lag-comp subsystem on BeginPlay (server). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|LagTest", meta = (AllowPrivateAccess = true))
	TObjectPtr<UAFLPawnHitboxHistoryComponent> HitboxHistory;

	/**
	 * Auto-detected by UAbilitySystemComponent::InitializeComponent's
	 * GetObjectsWithOuter scan. Pointer kept only to prevent GC before
	 * InitializeComponent runs — never dereferenced after construction.
	 */
	UPROPERTY()
	TObjectPtr<const UAFLAttributeSet_Combat> CombatSet;

	/** Captured in BeginPlay; the sinusoidal sweep oscillates around this. */
	FVector SpawnOrigin = FVector::ZeroVector;
};

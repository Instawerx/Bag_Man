// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"

#include "AFLDamageTarget.generated.h"

class ULyraAbilitySystemComponent;
class UAFLAttributeSet_Combat;
class UStaticMeshComponent;

/**
 * AAFLDamageTarget
 *
 * Self-contained damageable target for BM-0102b damage-verification PIE.
 * Plain AActor (deliberately NOT ALyraCharacter — sidesteps BM-DEBT-004's
 * UGameFeatureAction_AddAbilities CastChecked crash on unpossessed Lyra pawns).
 *
 * Pattern mirrors ALyraCharacterWithAbilities exactly: ASC + AttributeSet as
 * constructor CreateDefaultSubobject, InitAbilityActorInfo(self,self) in
 * PostInitializeComponents. Attribute-set auto-detection runs in
 * UAbilitySystemComponent::InitializeComponent via GetObjectsWithOuter +
 * Cast<UAttributeSet> + SpawnedAttributes.AddUnique (source-confirmed reliable
 * for default-subobject attribute sets owned by the same actor).
 *
 * Ctor-seeded values from UAFLAttributeSet_Combat: Health=100, MaxHealth=100,
 * OverkillThreshold=50, Shield/MaxShield/Armor=0, Heat=0/MaxHeat=100. No
 * BeginPlay GE-apply needed for initial state — the ctor already seeds.
 *
 * BP-side test stimulus (UGE_AFL_Damage_Pulse self-apply at T+2s) lives on the
 * BP child B_AFL_DamageTarget (BM-0102b Phase C-2 modifiability probe) — kept
 * off the C++ so this class stays a clean reusable target.
 */
UCLASS(Blueprintable)
class AFLCOMBAT_API AAFLDamageTarget : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAFLDamageTarget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	virtual void PostInitializeComponents() override;
	//~End of AActor interface

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Target", meta = (AllowPrivateAccess = true))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Self-contained Lyra ASC. SetIsReplicated + Mixed replication mode mirror ALyraCharacterWithAbilities. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Target", meta = (AllowPrivateAccess = true))
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * Auto-detected by UAbilitySystemComponent::InitializeComponent's
	 * GetObjectsWithOuter scan. Pointer kept here only to prevent GC before
	 * InitializeComponent runs — never dereferenced after construction.
	 */
	UPROPERTY()
	TObjectPtr<const UAFLAttributeSet_Combat> CombatSet;
};

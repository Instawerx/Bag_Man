// Copyright C12 AI Gaming. All Rights Reserved.

#include "Targeting/AFLDamageTarget.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Components/StaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDamageTarget)


AAFLDamageTarget::AAFLDamageTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	// ASC owned by THIS actor (self-contained), not by a PlayerState. Mirrors
	// ALyraCharacterWithAbilities (LyraCharacterWithAbilities.cpp:15-17). Mixed
	// replication mode is the canonical Lyra choice for ASCs that aren't
	// PlayerState-owned (the replication-mode rationale lives in Lyra's docs
	// and matches what LyraCharacterWithAbilities ships).
	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<ULyraAbilitySystemComponent>(
		this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// AttributeSet auto-detected by UAbilitySystemComponent::InitializeComponent's
	// GetObjectsWithOuter(Owner, ...) + Cast<UAttributeSet> scan (engine code
	// AbilitySystemComponent_Abilities.cpp:75-85). Ctor seeds Health=100,
	// MaxHealth=100, OverkillThreshold=50 — no BeginPlay GE-apply required for
	// initial state.
	CombatSet = CreateDefaultSubobject<UAFLAttributeSet_Combat>(TEXT("CombatSet"));

	// Lyra convention for ASC-bearing actors — matches LyraCharacterWithAbilities.cpp:24.
	SetNetUpdateFrequency(100.0f);
}

void AAFLDamageTarget::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Belt-and-braces re-init. UAbilitySystemComponent::InitializeComponent
	// already called InitAbilityActorInfo(Owner, Owner) before this hook runs
	// (see AbilitySystemComponent_Abilities.cpp:64), but the explicit re-call
	// mirrors LyraCharacterWithAbilities.cpp:32 and ensures AbilityActorInfo is
	// fully bound with both owner and avatar = this actor.
	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

UAbilitySystemComponent* AAFLDamageTarget::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

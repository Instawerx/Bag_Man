// Copyright C12 AI Gaming. All Rights Reserved.

#include "Targeting/AFLDamageTargetSkeletal.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDamageTargetSkeletal)

AAFLDamageTargetSkeletal::AAFLDamageTargetSkeletal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<ULyraAbilitySystemComponent>(
		this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	CombatSet = CreateDefaultSubobject<UAFLAttributeSet_Combat>(TEXT("CombatSet"));

	SetNetUpdateFrequency(100.0f);
}

void AAFLDamageTargetSkeletal::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

UAbilitySystemComponent* AAFLDamageTargetSkeletal::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

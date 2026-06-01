// Copyright C12 AI Gaming. All Rights Reserved.

#include "Targeting/AFLTargetDummy.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Combat/AFLDeathComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLTargetDummy)

AAFLTargetDummy::AAFLTargetDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The reusable AFL death driver -- subobject so every placed dummy carries it. The AFL
	// combat set itself is granted by the experience AddAbilities entry keyed on this class
	// (DA_AFL_Combat_AbilitySet), the proven 1a path; the death component binds it at BeginPlay.
	DeathComponent = CreateDefaultSubobject<UAFLDeathComponent>(TEXT("AFLDeathComponent"));

	// No AI; static target. AutoPossess stays disabled (set on the placed instance / BP).
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AAFLTargetDummy::BeginPlay()
{
	Super::BeginPlay();

	// Bind the per-hit react to the AFL combat set's OnHealthChanged. (Death is handled by
	// UAFLDeathComponent off OnOutOfHealth -- this is only the cosmetic react.) ASC is the
	// self-owned ULyraASC from ALyraCharacterWithAbilities, ready by now.
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(this))
	{
		CombatSet = ASC->GetSet<UAFLAttributeSet_Combat>();
		if (CombatSet)
		{
			HealthChangedHandle = CombatSet->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
		}
	}
}

void AAFLTargetDummy::HandleHealthChanged(AActor* /*Instigator*/, AActor* /*Causer*/, float Magnitude)
{
	// Magnitude is the signed Health delta; only react to actual damage (negative).
	if (Magnitude < 0.0f)
	{
		OnDamageReact(-Magnitude);   // hand the BP a positive damage value for the flash/montage
	}
}

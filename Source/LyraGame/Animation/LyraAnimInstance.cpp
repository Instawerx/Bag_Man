// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraAnimInstance.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraCharacter.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "Containers/Ticker.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "LyraLogChannels.h"
#include "Weapons/LyraWeaponInstance.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAnimInstance)


ULyraAnimInstance::ULyraAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULyraAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	GameplayTagPropertyMap.Initialize(this, ASC);
}

#if WITH_EDITOR
EDataValidationResult ULyraAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	GameplayTagPropertyMap.IsDataValid(this, Context);

	return ((Context.GetNumErrors() > 0) ? EDataValidationResult::Invalid : EDataValidationResult::Valid);
}
#endif // WITH_EDITOR

void ULyraAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (AActor* OwningActor = GetOwningActor())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
		{
			InitializeWithAbilitySystem(ASC);
		}
	}

	// This runs again every time the instance is RE-initialized in place (physics-asset reinit,
	// mesh churn) — and a re-init silently drops every linked anim layer, leaving the pawn at the
	// reference pose while its equipped weapon believes its layer is linked (the chronic client
	// A-pose). Re-drive the equipped weapon's state-keyed link next tick, after the re-init
	// settles. Measured: the equip-time link lands and verifies, then a later same-instance
	// re-init wipes the LinkedInstances array with no other observable signal.
	APawn* PawnOwner = TryGetPawnOwner();
	UWorld* World = PawnOwner ? PawnOwner->GetWorld() : nullptr;
	if (PawnOwner && World && World->IsGameWorld())
	{
		UE_LOG(LogLyra, Log, TEXT("AFL_ANIMRELINK: anim (re)init id=%u on %s -- scheduling relink."),
			GetUniqueID(), *GetNameSafe(PawnOwner));
		TWeakObjectPtr<APawn> WeakPawn(PawnOwner);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[WeakPawn](float) -> bool
			{
				if (APawn* Pawn = WeakPawn.Get())
				{
					if (ULyraEquipmentManagerComponent* EM = Pawn->FindComponentByClass<ULyraEquipmentManagerComponent>())
					{
						if (ULyraWeaponInstance* Weapon = Cast<ULyraWeaponInstance>(EM->GetFirstInstanceOfType(ULyraWeaponInstance::StaticClass())))
						{
							Weapon->TryLinkEquippedAnimLayer();
						}
					}
				}
				return false; // one-shot
			}), 0.0f);
	}
}

void ULyraAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ALyraCharacter* Character = Cast<ALyraCharacter>(GetOwningActor());
	if (!Character)
	{
		return;
	}

	ULyraCharacterMovementComponent* CharMoveComp = CastChecked<ULyraCharacterMovementComponent>(Character->GetCharacterMovement());
	const FLyraCharacterGroundInfo& GroundInfo = CharMoveComp->GetGroundInfo();
	GroundDistance = GroundInfo.GroundDistance;
}


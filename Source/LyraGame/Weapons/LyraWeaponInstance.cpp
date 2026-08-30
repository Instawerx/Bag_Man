// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWeaponInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/LyraPawnComponent_CharacterParts.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "LyraLogChannels.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/InputDeviceProperties.h"
#include "Character/LyraHealthComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWeaponInstance)

class UAnimInstance;
struct FGameplayTagContainer;

ULyraWeaponInstance::ULyraWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Listen for death of the owning pawn so that any device properties can be removed if we
	// die and can't unequip
	if (APawn* Pawn = GetPawn())
	{
		// We only need to do this for player controlled pawns, since AI and others won't have input devices on the client
		if (Pawn->IsPlayerControlled())
		{
			if (ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(GetPawn()))
			{
				HealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
			}
		}
	}
}

void ULyraWeaponInstance::OnEquipped()
{
	Super::OnEquipped();

	UWorld* World = GetWorld();
	check(World);
	TimeLastEquipped = World->GetTimeSeconds();

	ApplyDeviceProperties();

	// A character-part rebuild after this point wipes the linked anim layer (see the header) —
	// watch for rebuilds while equipped so the layer survives cosmetics streaming in late.
	if (APawn* Pawn = GetPawn())
	{
		ULyraPawnComponent_CharacterParts* PartsComp = Pawn->FindComponentByClass<ULyraPawnComponent_CharacterParts>();
		if (PartsComp)
		{
			WatchedCharacterParts = PartsComp;
			PartsComp->OnCharacterPartsChanged.AddUniqueDynamic(this, &ThisClass::OnCosmeticPartsChanged);
		}
		const ACharacter* Ch = Cast<ACharacter>(Pawn);
		const USkeletalMeshComponent* Mesh = Ch ? Ch->GetMesh() : nullptr;
		UE_LOG(LogLyra, Log, TEXT("AFL_ANIMRELINK: OnEquipped %s role=%d partsComp=%s meshAsset=%s animInst=%s#%u"),
			*GetNameSafe(this), (int32)Pawn->GetLocalRole(), PartsComp ? TEXT("bound") : TEXT("MISSING"),
			Mesh ? *GetNameSafe(Mesh->GetSkeletalMeshAsset()) : TEXT("<nomesh>"),
			Mesh ? *GetNameSafe(Mesh->GetAnimInstance() ? Mesh->GetAnimInstance()->GetClass() : nullptr) : TEXT("<nomesh>"),
			(Mesh && Mesh->GetAnimInstance()) ? Mesh->GetAnimInstance()->GetUniqueID() : 0);
	}

	// KEY ON STATE, NOT DELIVERY ORDER (same doctrine as the cosmetic-selection defer): if the
	// tags-selected layer cannot link yet, keep trying until it can. This is what actually cures
	// the client A-pose — no replication ordering assumption survives contact with a fresh client.
	LinkRetryAttempts = 0;
	if (!TryLinkEquippedAnimLayer())
	{
		TWeakObjectPtr<ULyraWeaponInstance> WeakThis(this);
		LinkRetryTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[WeakThis](float) -> bool
			{
				ULyraWeaponInstance* Self = WeakThis.Get();
				return Self ? !Self->TryLinkEquippedAnimLayer() : false;
			}), 0.25f);
	}
}

void ULyraWeaponInstance::OnUnequipped()
{
	Super::OnUnequipped();

	RemoveDeviceProperties();

	if (ULyraPawnComponent_CharacterParts* PartsComp = WatchedCharacterParts.Get())
	{
		PartsComp->OnCharacterPartsChanged.RemoveDynamic(this, &ThisClass::OnCosmeticPartsChanged);
	}
	WatchedCharacterParts.Reset();

	if (LinkRetryTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LinkRetryTicker);
		LinkRetryTicker.Reset();
	}
}

bool ULyraWeaponInstance::TryLinkEquippedAnimLayer()
{
	++LinkRetryAttempts;
	const bool bLogThis = (LinkRetryAttempts == 1) || (LinkRetryAttempts % 8 == 0);

	ACharacter* Character = Cast<ACharacter>(GetPawn());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || !Mesh->GetAnimInstance())
	{
		if (bLogThis)
		{
			UE_LOG(LogLyra, Log, TEXT("AFL_ANIMRELINK: attempt %d -- no mesh/anim instance yet on %s."),
				LinkRetryAttempts, *GetNameSafe(Character));
		}
		return false;
	}

	FGameplayTagContainer CosmeticTags;
	if (ULyraPawnComponent_CharacterParts* PartsComp = Character->FindComponentByClass<ULyraPawnComponent_CharacterParts>())
	{
		// Unfiltered on purpose: rules match RequiredTags-subset, so the superset can only widen the
		// match -- and it is the exact query the measured relink control proved against.
		CosmeticTags = PartsComp->GetCombinedTags(FGameplayTag());
	}

	const TSubclassOf<UAnimInstance> AnimLayer = PickBestAnimLayer(/*bEquipped=*/ true, CosmeticTags);
	if (!AnimLayer)
	{
		if (bLogThis)
		{
			UE_LOG(LogLyra, Log, TEXT("AFL_ANIMRELINK: attempt %d -- no layer selectable yet (tags=%d)."),
				LinkRetryAttempts, CosmeticTags.Num());
		}
		return false;
	}

	// The component's LinkedInstances array is what actually corresponds to a working pose (the
	// layer-NODE query can name a target instance while the array — and the animation — has
	// nothing). Check and VERIFY against the array; success is only a link that demonstrably took.
	const USkeletalMeshComponent* CMesh = Mesh;
	auto IsInLinkedArray = [CMesh](const UClass* InClass) -> bool
	{
		for (const UAnimInstance* L : CMesh->GetLinkedAnimInstances())
		{
			if (L && L->GetClass() == InClass)
			{
				return true;
			}
		}
		return false;
	};

	if (!IsInLinkedArray(*AnimLayer))
	{
		Mesh->LinkAnimClassLayers(AnimLayer);
	}

	const bool bLinked = IsInLinkedArray(*AnimLayer);
	if (bLinked)
	{
		UE_LOG(LogLyra, Log, TEXT("AFL_ANIMRELINK: linked %s on %s (attempt=%d tags=%d)."),
			*GetNameSafe(*AnimLayer), *GetNameSafe(Character), LinkRetryAttempts, CosmeticTags.Num());
	}
	else if (bLogThis)
	{
		UE_LOG(LogLyra, Log,
			TEXT("AFL_ANIMRELINK: attempt %d -- link did NOT take (layer=%s nodeTarget=%s linkedNum=%d rendered=%d)."),
			LinkRetryAttempts, *GetNameSafe(*AnimLayer),
			*GetNameSafe(Mesh->GetLinkedAnimLayerInstanceByClass(AnimLayer)),
			CMesh->GetLinkedAnimInstances().Num(), Mesh->bRecentlyRendered ? 1 : 0);
	}
	return bLinked;
}

void ULyraWeaponInstance::OnCosmeticPartsChanged(ULyraPawnComponent_CharacterParts* ComponentWithChangedParts)
{
	// A part rebuild can swap the body mesh (recreating the AnimInstance, dropping linked layers)
	// or change the anim-style tags -- either way, re-drive the same state-keyed link.
	TryLinkEquippedAnimLayer();
}

void ULyraWeaponInstance::UpdateFiringTime()
{
	UWorld* World = GetWorld();
	check(World);
	TimeLastFired = World->GetTimeSeconds();
}

float ULyraWeaponInstance::GetTimeSinceLastInteractedWith() const
{
	UWorld* World = GetWorld();
	check(World);
	const double WorldTime = World->GetTimeSeconds();

	double Result = WorldTime - TimeLastEquipped;

	if (TimeLastFired > 0.0)
	{
		const double TimeSinceFired = WorldTime - TimeLastFired;
		Result = FMath::Min(Result, TimeSinceFired);
	}

	return Result;
}

TSubclassOf<UAnimInstance> ULyraWeaponInstance::PickBestAnimLayer(bool bEquipped, const FGameplayTagContainer& CosmeticTags) const
{
	const FLyraAnimLayerSelectionSet& SetToQuery = (bEquipped ? EquippedAnimSet : UneuippedAnimSet);
	return SetToQuery.SelectBestLayer(CosmeticTags);
}

const FPlatformUserId ULyraWeaponInstance::GetOwningUserId() const
{
	if (const APawn* Pawn = GetPawn())
	{
		return Pawn->GetPlatformUserId();
	}
	return PLATFORMUSERID_NONE;
}

void ULyraWeaponInstance::ApplyDeviceProperties()
{
	const FPlatformUserId UserId = GetOwningUserId();

	if (UserId.IsValid())
	{
		if (UInputDeviceSubsystem* InputDeviceSubsystem = UInputDeviceSubsystem::Get())
		{
			for (TObjectPtr<UInputDeviceProperty>& DeviceProp : ApplicableDeviceProperties)
			{
				FActivateDevicePropertyParams Params = {};
				Params.UserId = UserId;

				// By default, the device property will be played on the Platform User's Primary Input Device.
				// If you want to override this and set a specific device, then you can set the DeviceId parameter.
				//Params.DeviceId = <some specific device id>;
				
				// Don't remove this property it was evaluated. We want the properties to be applied as long as we are holding the 
				// weapon, and will remove them manually in OnUnequipped
				Params.bLooping = true;
			
				DevicePropertyHandles.Emplace(InputDeviceSubsystem->ActivateDeviceProperty(DeviceProp, Params));
			}
		}	
	}
}

void ULyraWeaponInstance::RemoveDeviceProperties()
{
	const FPlatformUserId UserId = GetOwningUserId();
	
	if (UserId.IsValid() && !DevicePropertyHandles.IsEmpty())
	{
		// Remove any device properties that have been applied
		if (UInputDeviceSubsystem* InputDeviceSubsystem = UInputDeviceSubsystem::Get())
		{
			InputDeviceSubsystem->RemoveDevicePropertyHandles(DevicePropertyHandles);
			DevicePropertyHandles.Empty();
		}
	}
}

void ULyraWeaponInstance::OnDeathStarted(AActor* OwningActor)
{
	// Remove any possibly active device properties when we die to make sure that there aren't any lingering around
	RemoveDeviceProperties();
}

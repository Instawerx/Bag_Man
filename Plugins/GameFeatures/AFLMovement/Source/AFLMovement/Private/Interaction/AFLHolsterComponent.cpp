// Copyright C12 AI Gaming. All Rights Reserved.

#include "Interaction/AFLHolsterComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHolsterComponent)

UAFLHolsterComponent::UAFLHolsterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAFLHolsterComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLHolsterComponent, bHolstered);
}

void UAFLHolsterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HolsteredGEHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
		{
			ASC->RemoveActiveGameplayEffect(HolsteredGEHandle);
		}
		HolsteredGEHandle.Invalidate();
	}
	Super::EndPlay(EndPlayReason);
}

void UAFLHolsterComponent::AddHolsterReason(FGameplayTag Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Reason.IsValid())
	{
		return; // server-authoritative
	}
	HolsterReasons.Add(Reason);
	UpdateHolsterState();
}

void UAFLHolsterComponent::RemoveHolsterReason(FGameplayTag Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Reason.IsValid())
	{
		return;
	}
	HolsterReasons.Remove(Reason);
	UpdateHolsterState();
}

void UAFLHolsterComponent::UpdateHolsterState()
{
	const bool bDesired = HolsterReasons.Num() > 0;
	if (bDesired == bHolstered)
	{
		return;
	}
	bHolstered = bDesired;

	// Apply / remove the State.Weapon.Holstered GE (server; its granted tag replicates to clients).
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (bHolstered && HolsteredEffectClass)
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddInstigator(GetOwner(), GetOwner());
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(HolsteredEffectClass, 1.0f, Context);
			if (Spec.IsValid())
			{
				HolsteredGEHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
		else if (!bHolstered && HolsteredGEHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(HolsteredGEHandle);
			HolsteredGEHandle.Invalidate();
		}
	}

	// Server does its own reattach (OnRep only fires on remotes).
	ApplyHolsterVisual(bHolstered);

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_HOLSTER: %s -> holstered=%d (reasons=%d)."),
		*GetNameSafe(GetOwner()), bHolstered ? 1 : 0, HolsterReasons.Num());
}

void UAFLHolsterComponent::OnRep_Holstered()
{
	ApplyHolsterVisual(bHolstered);
}

void UAFLHolsterComponent::ApplyHolsterVisual(bool bNowHolstered)
{
	ULyraEquipmentManagerComponent* EquipMgr = GetEquipmentManager();
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (!EquipMgr || !Mesh)
	{
		OnHolsterStateChanged(bNowHolstered); // pose hook must still fire (relink) even with no weapon to reattach
		return;
	}

	for (ULyraEquipmentInstance* Instance : EquipMgr->GetEquipmentInstancesOfType(ULyraEquipmentInstance::StaticClass()))
	{
		if (!Instance)
		{
			continue;
		}
		for (AActor* WeaponActor : Instance->GetSpawnedActors())
		{
			USceneComponent* Root = WeaponActor ? WeaponActor->GetRootComponent() : nullptr;
			if (!Root)
			{
				continue;
			}

			if (bNowHolstered)
			{
				// Cache the current (hand) attach so the draw restores it exactly, then snap to the back socket.
				FCachedAttach Cache;
				Cache.Parent = Root->GetAttachParent();
				Cache.Socket = Root->GetAttachSocketName();
				Cache.RelativeTransform = Root->GetRelativeTransform();
				CachedAttachments.Add(WeaponActor, Cache);

				WeaponActor->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetIncludingScale, BackSocket);
			}
			else if (const FCachedAttach* Cache = CachedAttachments.Find(WeaponActor))
			{
				// Draw: restore the original hand attach (parent + socket + relative transform).
				if (USceneComponent* OriginalParent = Cache->Parent.Get())
				{
					WeaponActor->AttachToComponent(OriginalParent, FAttachmentTransformRules::KeepRelativeTransform, Cache->Socket);
					Root->SetRelativeTransform(Cache->RelativeTransform);
				}
				CachedAttachments.Remove(WeaponActor);
			}
		}
	}

	// Pose hook: the BP child (BPC_AFL_Holster) relinks the mesh anim layer (weapon <-> ABP_UnarmedAnimLayers)
	// so the HANDS match the holstered weapon, and/or plays the draw/holster montage. Runs on server + clients
	// (via OnRep_Holstered) so every proxy poses correctly.
	OnHolsterStateChanged(bNowHolstered);
}

ULyraEquipmentManagerComponent* UAFLHolsterComponent::GetEquipmentManager() const
{
	// EquipmentManager lives on the PlayerState for this hero (the ranged-weapon pattern the climb/grab holster
	// used); fall back to Pawn / Controller for robustness across possession ordering.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return nullptr;
	}
	if (AActor* PS = Cast<AActor>(Pawn->GetPlayerState()))
	{
		if (ULyraEquipmentManagerComponent* Mgr = PS->FindComponentByClass<ULyraEquipmentManagerComponent>())
		{
			return Mgr;
		}
	}
	if (ULyraEquipmentManagerComponent* Mgr = Pawn->FindComponentByClass<ULyraEquipmentManagerComponent>())
	{
		return Mgr;
	}
	if (AController* Controller = Pawn->GetController())
	{
		return Controller->FindComponentByClass<ULyraEquipmentManagerComponent>();
	}
	return nullptr;
}

USkeletalMeshComponent* UAFLHolsterComponent::GetOwnerMesh() const
{
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		return Char->GetMesh();
	}
	return nullptr;
}

// Copyright C12 AI Gaming. All Rights Reserved.

#include "HUD/AFLHitConfirmComponent.h"

#include "AFLCombat.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HUD/AFLCameraShake_HitConfirm.h"
#include "HUD/AFLHitConfirmMessage.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHitConfirmComponent)


// Tag declared in AFLCombatTags.ini (Event.Damage.Confirmed). Native-define
// here as well so first-boot module init does not race the per-plugin tag-ini
// scan when the listener registration runs. Matches the pattern used by
// AFLAG_Laser_Pulse for State.Firing.Pulse.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_Confirmed, "Event.Damage.Confirmed");


UAFLHitConfirmComponent::UAFLHitConfirmComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	CameraShakeClass = UAFLCameraShake_HitConfirm::StaticClass();

	// Default heuristic until AFL-0411 publishes the body-zone enum. ALyraCharacter
	// uses the UE5 mannequin skeleton: "head" matches the head bone exactly and
	// "neck_01" is caught via substring contains.
	HeadshotBoneFragments = { FName(TEXT("head")), FName(TEXT("neck")) };
}

void UAFLHitConfirmComponent::BeginPlay()
{
	Super::BeginPlay();

	const APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	ListenerHandle = MessageSubsystem.RegisterListener(
		TAG_Event_Damage_Confirmed,
		this,
		&ThisClass::OnDamageConfirmed);
}

void UAFLHitConfirmComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ListenerHandle.IsValid())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
		MessageSubsystem.UnregisterListener(ListenerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UAFLHitConfirmComponent::OnDamageConfirmed(FGameplayTag Channel, const FAFLHitConfirmMessage& Payload)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// Filter: only react to confirms WE caused. Multiple firing clients share
	// the same channel; without this filter every player sees every hitmarker
	// in the lobby. Instigator is set by the ExecCalc to the EffectContext
	// causer (the firing pawn) — or its outer Instigator if the causer is a
	// weapon actor.
	const AActor* InstigatorActor = Cast<AActor>(Payload.Instigator);
	const APawn* OwnedPawn = PC->GetPawn();
	const bool bWeInstigated =
		(InstigatorActor == OwnedPawn) ||
		(InstigatorActor && OwnedPawn && InstigatorActor->GetInstigator() == OwnedPawn);
	if (!bWeInstigated)
	{
		return;
	}

	FAFLHitConfirmInfo Info;
	Info.Damage     = Payload.Damage;
	Info.BoneName   = Payload.BoneName;
	Info.DistanceCm = Payload.DistanceCm;
	Info.bHeadshot  = IsHeadshotBone(Info.BoneName);

	// 1. Camera shake. ClientStartCameraShake handles null gracefully but we
	//    guard so a misconfigured BP doesn't log a warning every shot.
	if (CameraShakeClass)
	{
		PC->ClientStartCameraShake(CameraShakeClass, /*Scale=*/1.0f);
	}

	// 2. Broadcast to UI listeners (WBP_AFL_HitMarker, AFL-0204b damage numbers).
	OnHitConfirmed.Broadcast(Info);

	UE_LOG(LogAFLCombat, Verbose,
		TEXT("hit_confirmed damage=%.1f zone=%s headshot=%d distance=%.0f"),
		Info.Damage,
		*Info.BoneName.ToString(),
		Info.bHeadshot ? 1 : 0,
		Info.DistanceCm);
}

APlayerController* UAFLHitConfirmComponent::GetOwningPlayerController() const
{
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		return PC;
	}
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		return Cast<APlayerController>(OwnerPawn->GetController());
	}
	return nullptr;
}

bool UAFLHitConfirmComponent::IsHeadshotBone(FName BoneName) const
{
	if (BoneName.IsNone())
	{
		return false;
	}

	const FString BoneString = BoneName.ToString();
	for (const FName& Fragment : HeadshotBoneFragments)
	{
		if (Fragment.IsNone())
		{
			continue;
		}
		if (BoneString.Contains(Fragment.ToString(), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

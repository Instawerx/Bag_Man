// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLOverkillListener.h"

#include "AFLCombat.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Messages/LyraVerbMessage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLOverkillListener)


static const FName NAME_Event_Damage_Overkill = TEXT("Event.Damage.Overkill");


bool UAFLOverkillListener::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only spawn for game worlds (PIE / runtime), not editor preview / inactive worlds.
	// Also require a GameInstance — UGameplayMessageSubsystem is a GameInstance subsystem
	// and Get() asserts when the World has no GameInstance. Transient test worlds created
	// via UWorld::CreateWorld(EWorldType::Game, false) hit that path without a GameInstance;
	// guarding here lets the listener cleanly skip those rather than crash.
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld() && World->GetGameInstance() != nullptr;
	}
	return false;
}

void UAFLOverkillListener::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Server-only: the ExecCalc broadcast fires inside WITH_SERVER_CODE. Skip
	// registration on dedicated clients so we don't allocate listener storage
	// for an event that will never arrive.
	if (World->GetNetMode() == NM_Client)
	{
		return;
	}

	const FGameplayTag OverkillTag = FGameplayTag::RequestGameplayTag(NAME_Event_Damage_Overkill, false);
	if (!OverkillTag.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("UAFLOverkillListener: Event.Damage.Overkill tag not registered; listener inactive."));
		return;
	}

	ListenerHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
		OverkillTag,
		this,
		&UAFLOverkillListener::OnOverkill);

	UE_LOG(LogAFLCombat, Log, TEXT("UAFLOverkillListener registered on Event.Damage.Overkill"));
}

void UAFLOverkillListener::Deinitialize()
{
	if (ListenerHandle.IsValid())
	{
		ListenerHandle.Unregister();
	}
	Super::Deinitialize();
}

void UAFLOverkillListener::OnOverkill(FGameplayTag Channel, const FLyraVerbMessage& Message)
{
	UE_LOG(LogAFLCombat, Display,
		TEXT("[Overkill] Instigator=%s Target=%s Magnitude=%.1f"),
		*GetNameSafe(Message.Instigator),
		*GetNameSafe(Message.Target),
		Message.Magnitude);
}

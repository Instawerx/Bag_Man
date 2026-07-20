// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "WeaponSpawns/AFLWeaponSpawnRegistry.h"

#include "AFLGameCore.h"                     // LogAFLGameCore
#include "Engine/World.h"
#include "Equipment/LyraPickupDefinition.h"  // ULyraWeaponPickupDefinition (TObjectPtr property completeness)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLWeaponSpawnRegistry)

void UAFLWeaponSpawnRegistry::RegisterMappings(const TMap<FGameplayTag, TObjectPtr<ULyraWeaponPickupDefinition>>& InMappings)
{
	Mappings = InMappings;
	bActive = true;
	UE_LOG(LogAFLGameCore, Log, TEXT("UAFLWeaponSpawnRegistry::RegisterMappings: %d entr(ies) installed in world '%s' -> broadcasting OnReady."),
		Mappings.Num(), *GetNameSafe(GetWorld()));
	OnReady.Broadcast();
}

void UAFLWeaponSpawnRegistry::Clear()
{
	Mappings.Reset();
	bActive = false;
	OnCleared.Broadcast();
}

ULyraWeaponPickupDefinition* UAFLWeaponSpawnRegistry::Resolve(const FGameplayTag& IntentTag) const
{
	if (!IntentTag.IsValid())
	{
		return nullptr;
	}

	const TObjectPtr<ULyraWeaponPickupDefinition>* Found = Mappings.Find(IntentTag);
	return Found ? *Found : nullptr;
}

bool UAFLWeaponSpawnRegistry::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLEconomyPersistenceSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogAFLEconPersist, Log, All);

namespace
{
	/** Single disk slot for all local economic state (records keyed inside by player id). */
	static const TCHAR* const GEconomySlot = TEXT("AFLEconomy");
	static constexpr int32 GEconomyUserIndex = 0;

	/** Stable key used when ForceLocalSlot is on (A0 default) or the incoming id is invalid. */
	static const TCHAR* const GLocalKeyBacking = TEXT("AFL.Local.Default");
}

// A0: force every persistence key to one stable local slot so the logout/login proof is deterministic
// even with an ephemeral PIE net-id. A1 sets this 0 once PlayFab login provides a real account id.
static TAutoConsoleVariable<int32> CVarEconForceLocalSlot(
	TEXT("afl.Econ.ForceLocalSlot"),
	1,
	TEXT("Phase A0: collapse all economy-persistence keys to one stable local slot (deterministic PIE logout/login proof). Set 0 at A1 once login provides a real account id."),
	ECVF_Default);

UAFLEconomyPersistenceSubsystem* UAFLEconomyPersistenceSubsystem::Get(const UObject* WorldContext)
{
	if (WorldContext)
	{
		if (const UWorld* World = WorldContext->GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				return GI->GetSubsystem<UAFLEconomyPersistenceSubsystem>();
			}
		}
	}
	return nullptr;
}

void UAFLEconomyPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsureLoaded();
	UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] Subsystem online -- Phase A0 local SaveGame backing (NOT anti-spoof; A1 = Lambda server-auth)."));
}

void UAFLEconomyPersistenceSubsystem::Deinitialize()
{
	// Final safety flush (fire-and-forget saves already hit disk; this covers any last mutation).
	Flush();
	Super::Deinitialize();
}

void UAFLEconomyPersistenceSubsystem::EnsureLoaded()
{
	if (SaveData)
	{
		return;
	}

	if (UGameplayStatics::DoesSaveGameExist(GEconomySlot, GEconomyUserIndex))
	{
		SaveData = Cast<UAFLEconomySaveGame>(UGameplayStatics::LoadGameFromSlot(GEconomySlot, GEconomyUserIndex));
	}

	if (!SaveData)
	{
		SaveData = Cast<UAFLEconomySaveGame>(UGameplayStatics::CreateSaveGameObject(UAFLEconomySaveGame::StaticClass()));
	}

	UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] EnsureLoaded: %s (records=%d)"),
		SaveData ? TEXT("ready") : TEXT("FAILED"),
		SaveData ? SaveData->Records.Num() : -1);
}

void UAFLEconomyPersistenceSubsystem::Flush() const
{
	if (SaveData)
	{
		const bool bOk = UGameplayStatics::SaveGameToSlot(SaveData, GEconomySlot, GEconomyUserIndex);
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] Flush -> slot '%s' %s (records=%d)"),
			GEconomySlot, bOk ? TEXT("OK") : TEXT("FAIL"), SaveData->Records.Num());
	}
}

FAFLPlayerId UAFLEconomyPersistenceSubsystem::ResolveKey(const FAFLPlayerId& In) const
{
	if (CVarEconForceLocalSlot.GetValueOnGameThread() != 0 || !In.IsValid())
	{
		return FAFLPlayerId::MakeFromBacking(GLocalKeyBacking);
	}
	return In;
}

FAFLEconomyRecord& UAFLEconomyPersistenceSubsystem::RecordFor(const FAFLPlayerId& Player)
{
	EnsureLoaded();
	return SaveData->Records.FindOrAdd(ResolveKey(Player));
}

//~ IAFLCosmeticPersistence ---------------------------------------------------------------------------

void UAFLEconomyPersistenceSubsystem::LoadBalance(const FAFLPlayerId& Player, FAFLOnBalanceLoaded OnLoaded)
{
	EnsureLoaded();
	if (const FAFLEconomyRecord* Rec = SaveData->Records.Find(ResolveKey(Player)))
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadBalance HIT V=%d W=%d"), Rec->Volts, Rec->Watts);
		OnLoaded.ExecuteIfBound(Rec->bHasBalance, Rec->Volts, Rec->Watts);
	}
	else
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadBalance MISS (new player) -> seed defaults"));
		OnLoaded.ExecuteIfBound(false, 0, 0);
	}
}

void UAFLEconomyPersistenceSubsystem::SaveBalance(const FAFLPlayerId& Player, int32 Volts, int32 Watts)
{
	FAFLEconomyRecord& Rec = RecordFor(Player);
	Rec.Volts = Volts;
	Rec.Watts = Watts;
	Rec.bHasBalance = true;
	UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] SaveBalance V=%d W=%d"), Volts, Watts);
	Flush();
}

void UAFLEconomyPersistenceSubsystem::LoadOwnedSet(const FAFLPlayerId& Player, FAFLOnOwnedSetLoaded OnLoaded)
{
	EnsureLoaded();
	if (const FAFLEconomyRecord* Rec = SaveData->Records.Find(ResolveKey(Player)))
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadOwnedSet HIT count=%d"), Rec->OwnedCosmeticIds.Num());
		OnLoaded.ExecuteIfBound(true, Rec->OwnedCosmeticIds);
	}
	else
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadOwnedSet MISS (new player)"));
		OnLoaded.ExecuteIfBound(false, TArray<FName>());
	}
}

void UAFLEconomyPersistenceSubsystem::SaveOwnedSet(const FAFLPlayerId& Player, const TArray<FName>& OwnedCosmeticIds)
{
	FAFLEconomyRecord& Rec = RecordFor(Player);
	Rec.OwnedCosmeticIds = OwnedCosmeticIds;
	UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] SaveOwnedSet count=%d"), OwnedCosmeticIds.Num());
	Flush();
}

void UAFLEconomyPersistenceSubsystem::LoadSelection(const FAFLPlayerId& Player, FAFLOnSelectionLoaded OnLoaded)
{
	EnsureLoaded();
	if (const FAFLEconomyRecord* Rec = SaveData->Records.Find(ResolveKey(Player)))
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadSelection HIT (bHasSelection=%d)"), Rec->bHasSelection ? 1 : 0);
		OnLoaded.ExecuteIfBound(Rec->bHasSelection, Rec->Selection);
	}
	else
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadSelection MISS (new player)"));
		OnLoaded.ExecuteIfBound(false, FAFLCosmeticSelection());
	}
}

void UAFLEconomyPersistenceSubsystem::SaveSelection(const FAFLPlayerId& Player, const FAFLCosmeticSelection& Selection)
{
	FAFLEconomyRecord& Rec = RecordFor(Player);
	Rec.Selection = Selection;
	Rec.bHasSelection = true;
	UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] SaveSelection"));
	Flush();
}

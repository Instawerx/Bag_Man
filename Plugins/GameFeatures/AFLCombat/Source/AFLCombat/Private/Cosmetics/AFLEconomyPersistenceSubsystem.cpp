// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLEconomyPersistenceSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

#include "AFLOnlineSubsystem.h"   // A1.1: PlayFab login + REST transport (the LOAD-from-PlayFab path)
#include "Dom/JsonObject.h"       // parse GetUserInventory (VirtualCurrency + Inventory)
#include "Misc/Guid.h"            // Phase 1 write-side: earn nonce (moved from the wallet)
#include "Misc/DateTime.h"        // Phase 1 write-side: earn ts (moved from the wallet)

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

// A1.1: LOAD balance/owned from PlayFab (the player's own token) when logged in; else the local cache
// (A0 / offline last-known-good). 0 = force A0 local-only (bypass PlayFab entirely).
static TAutoConsoleVariable<int32> CVarEconUsePlayFab(
	TEXT("afl.Econ.UsePlayFab"),
	1,
	TEXT("Phase A1.1: LOAD balance/owned from PlayFab when logged in (player's own token); else the local cache. 0 = A0 local-only."),
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
	// A1.1: once logged in, the incoming key IS the durable PlayFabId (MakePlayerId returns it) -> honor it,
	// so the local cache is account-scoped (matches the PlayFab truth it mirrors).
	if (const UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this))
	{
		if (Online->IsLoggedIn() && In.IsValid())
		{
			return In;
		}
	}
	// Not logged in -> A0 behavior: ForceLocalSlot (default on) or an invalid id -> one stable local slot.
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

//~ A1.1 -- PlayFab LOAD path -------------------------------------------------------------------------

bool UAFLEconomyPersistenceSubsystem::ShouldUsePlayFab() const
{
	if (CVarEconUsePlayFab.GetValueOnGameThread() == 0) { return false; }
	return UAFLOnlineSubsystem::Get(this) != nullptr;
}

void UAFLEconomyPersistenceSubsystem::FetchInventoryFromPlayFab(const FAFLPlayerId& Player,
	TFunction<void(bool, int32, int32, const TArray<FName>&)> OnDone)
{
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online) { OnDone(false, 0, 0, TArray<FName>()); return; }

	TWeakObjectPtr<UAFLEconomyPersistenceSubsystem> WeakThis(this);
	Online->CallWhenLoggedIn([WeakThis, Player, OnDone](bool bLoggedIn)
	{
		UAFLEconomyPersistenceSubsystem* Self = WeakThis.Get();
		if (!Self) { return; }
		if (!bLoggedIn) { OnDone(false, 0, 0, TArray<FName>()); return; }

		UAFLOnlineSubsystem* O = UAFLOnlineSubsystem::Get(Self);
		if (!O) { OnDone(false, 0, 0, TArray<FName>()); return; }

		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		O->PostClientApi(TEXT("GetUserInventory"), Body,
			[WeakThis, Player, OnDone](bool bOk, TSharedPtr<FJsonObject> Data)
			{
				UAFLEconomyPersistenceSubsystem* S = WeakThis.Get();
				if (!S) { return; }
				if (!bOk || !Data.IsValid()) { OnDone(false, 0, 0, TArray<FName>()); return; }

				int32 VO = 0, WA = 0;
				TArray<FName> Owned;

				const TSharedPtr<FJsonObject>* VC = nullptr;
				if (Data->TryGetObjectField(TEXT("VirtualCurrency"), VC) && VC)
				{
					(*VC)->TryGetNumberField(TEXT("VO"), VO);
					(*VC)->TryGetNumberField(TEXT("WA"), WA);
				}

				const TArray<TSharedPtr<FJsonValue>>* Inv = nullptr;
				if (Data->TryGetArrayField(TEXT("Inventory"), Inv) && Inv)
				{
					for (const TSharedPtr<FJsonValue>& Item : *Inv)
					{
						const TSharedPtr<FJsonObject> Obj = Item.IsValid() ? Item->AsObject() : nullptr;
						FString ItemId;
						if (Obj.IsValid() && Obj->TryGetStringField(TEXT("ItemId"), ItemId) && !ItemId.IsEmpty())
						{
							Owned.Add(FName(*ItemId));
						}
					}
				}

				// Mirror the authoritative PlayFab state into the local cache (A1's offline last-known-good).
				S->EnsureLoaded();
				FAFLEconomyRecord& Rec = S->SaveData->Records.FindOrAdd(S->ResolveKey(Player));
				Rec.Volts = VO;
				Rec.Watts = WA;
				Rec.bHasBalance = true;
				Rec.OwnedCosmeticIds = Owned;
				S->Flush();

				UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] PlayFab GetUserInventory OK VO=%d WA=%d owned=%d (mirrored to cache)"), VO, WA, Owned.Num());
				OnDone(true, VO, WA, Owned);
			}, /*bRequireAuth*/ true);
	}, /*TimeoutSeconds*/ 6.0f);
}

void UAFLEconomyPersistenceSubsystem::ReadBalanceFromCache(const FAFLPlayerId& Player, FAFLOnBalanceLoaded OnLoaded)
{
	EnsureLoaded();
	if (const FAFLEconomyRecord* Rec = SaveData->Records.Find(ResolveKey(Player)))
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadBalance cache HIT V=%d W=%d"), Rec->Volts, Rec->Watts);
		OnLoaded.ExecuteIfBound(Rec->bHasBalance, Rec->Volts, Rec->Watts);
	}
	else
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadBalance cache MISS (new player) -> seed defaults"));
		OnLoaded.ExecuteIfBound(false, 0, 0);
	}
}

void UAFLEconomyPersistenceSubsystem::ReadOwnedFromCache(const FAFLPlayerId& Player, FAFLOnOwnedSetLoaded OnLoaded)
{
	EnsureLoaded();
	if (const FAFLEconomyRecord* Rec = SaveData->Records.Find(ResolveKey(Player)))
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadOwnedSet cache HIT count=%d"), Rec->OwnedCosmeticIds.Num());
		OnLoaded.ExecuteIfBound(true, Rec->OwnedCosmeticIds);
	}
	else
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadOwnedSet cache MISS (new player)"));
		OnLoaded.ExecuteIfBound(false, TArray<FName>());
	}
}

//~ Dev VERIFY harness (A1.1) -------------------------------------------------------------------------

void UAFLEconomyPersistenceSubsystem::DebugWipeLocalCache()
{
	if (UGameplayStatics::DoesSaveGameExist(GEconomySlot, GEconomyUserIndex))
	{
		UGameplayStatics::DeleteGameInSlot(GEconomySlot, GEconomyUserIndex);
	}
	SaveData = nullptr;   // drop the in-memory mirror too, so nothing masks a fresh load
	EnsureLoaded();        // recreate an empty SaveGame (records=0)
	UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] DebugWipeLocalCache -- slot '%s' deleted, cache reset to empty."), GEconomySlot);
}

void UAFLEconomyPersistenceSubsystem::DebugProbePlayFabLoad(
	TFunction<void(bool, const FString&, int32, int32, const TArray<FName>&, bool)> OnDone)
{
	if (!UAFLOnlineSubsystem::Get(this))
	{
		OnDone(false, FString(), 0, 0, TArray<FName>(), false);
		return;
	}
	TWeakObjectPtr<UAFLEconomyPersistenceSubsystem> WeakThis(this);
	FetchInventoryFromPlayFab(FAFLPlayerId(), [WeakThis, OnDone](bool bOk, int32 VO, int32 WA, const TArray<FName>& Owned)
	{
		UAFLEconomyPersistenceSubsystem* Self = WeakThis.Get();
		const UAFLOnlineSubsystem* O = Self ? UAFLOnlineSubsystem::Get(Self) : nullptr;
		const bool bLoginOk = (O != nullptr) && O->IsLoggedIn();
		const FString PfId = O ? O->GetPlayFabId() : FString();
		OnDone(bLoginOk, PfId, VO, WA, Owned, bOk);
	});
}

//~ IAFLCosmeticPersistence ---------------------------------------------------------------------------

void UAFLEconomyPersistenceSubsystem::LoadBalance(const FAFLPlayerId& Player, FAFLOnBalanceLoaded OnLoaded)
{
	EnsureLoaded();
	// A1.1: prefer PlayFab (authoritative server truth) when logged in; else the local cache (A0 / offline).
	if (ShouldUsePlayFab())
	{
		TWeakObjectPtr<UAFLEconomyPersistenceSubsystem> WeakThis(this);
		FetchInventoryFromPlayFab(Player, [WeakThis, Player, OnLoaded](bool bOk, int32 V, int32 W, const TArray<FName>&)
		{
			UAFLEconomyPersistenceSubsystem* Self = WeakThis.Get();
			if (!Self) { return; }
			if (bOk)
			{
				UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadBalance from PlayFab VO=%d WA=%d"), V, W);
				OnLoaded.ExecuteIfBound(true, V, W);
			}
			else
			{
				UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadBalance PlayFab miss/offline -> cache"));
				Self->ReadBalanceFromCache(Player, OnLoaded);
			}
		});
		return;
	}
	ReadBalanceFromCache(Player, OnLoaded);
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
	// A1.1: prefer PlayFab (authoritative owned-set) when logged in; else the local cache (A0 / offline).
	if (ShouldUsePlayFab())
	{
		TWeakObjectPtr<UAFLEconomyPersistenceSubsystem> WeakThis(this);
		FetchInventoryFromPlayFab(Player, [WeakThis, Player, OnLoaded](bool bOk, int32, int32, const TArray<FName>& Owned)
		{
			UAFLEconomyPersistenceSubsystem* Self = WeakThis.Get();
			if (!Self) { return; }
			if (bOk)
			{
				UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadOwnedSet from PlayFab count=%d"), Owned.Num());
				OnLoaded.ExecuteIfBound(true, Owned);
			}
			else
			{
				UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadOwnedSet PlayFab miss/offline -> cache"));
				Self->ReadOwnedFromCache(Player, OnLoaded);
			}
		});
		return;
	}
	ReadOwnedFromCache(Player, OnLoaded);
}

void UAFLEconomyPersistenceSubsystem::SaveOwnedSet(const FAFLPlayerId& Player, const TArray<FName>& OwnedCosmeticIds)
{
	FAFLEconomyRecord& Rec = RecordFor(Player);
	Rec.OwnedCosmeticIds = OwnedCosmeticIds;
	UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] SaveOwnedSet count=%d"), OwnedCosmeticIds.Num());
	Flush();
}

//~ S-ECON WRITE-SIDE (Phase 1): the two authoritative PlayFab TRANSACTIONS behind the seam ---------------------
// Refactor-behind-interface: the transport (body build + the /earn and Client/PurchaseItem calls) is MOVED here
// VERBATIM from the wallet's former inline path -- same endpoint, same body, same completion contract, same
// server-side anti-spoof. Zero behaviour change; only the call site moved (wallet -> seam -> here).

void UAFLEconomyPersistenceSubsystem::EarnThroughBackend(const FString& PlayFabId, const FString& CurrencyCode,
	int32 Amount, const FString& Reason, const FString& MatchId, FAFLOnEarnComplete OnComplete)
{
	// MOVED from UAFLWalletComponent::EarnWattsAuthority (:268-279). Same HMAC/dedicated//earn transport + body.
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online)
	{
		OnComplete.ExecuteIfBound(false, FString());
		return;
	}
	// (e) build the contract body (docs/earn-endpoint-contract.md): integer amount + ts, fresh server nonce.
	// Reason is the funnel's tag ("extraction" / "loot:*") -- a controlled internal literal (audit/dedupe only).
	const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	const int64 Ts = FDateTime::UtcNow().ToUnixTimestamp();
	const TCHAR* ReasonTag = (!Reason.IsEmpty()) ? *Reason : TEXT("earn");
	const FString Body = FString::Printf(
		TEXT("{\"playFabId\":\"%s\",\"currencyCode\":\"%s\",\"amount\":%d,\"reason\":\"%s\",\"matchId\":\"%s\",\"nonce\":\"%s\",\"ts\":%lld}"),
		*PlayFabId, *CurrencyCode, Amount, ReasonTag, *MatchId, *Nonce, static_cast<long long>(Ts));

	// (f) push to the server-authoritative /earn Lambda (A1.3b). PostServerEarn self-gates on the server env
	// key/URL; forward the result to the caller (which parses newBalance + logs AFL_A13S3 identically).
	Online->PostServerEarn(Body, [OnComplete](bool bOk, const FString& Resp)
	{
		OnComplete.ExecuteIfBound(bOk, Resp);
	});
}

void UAFLEconomyPersistenceSubsystem::PurchaseThroughBackend(FName CosmeticId, const FString& CurrencyCode,
	int32 Price, FAFLOnPurchaseComplete OnComplete)
{
	// MOVED from UAFLWalletComponent::ClientRequestPurchase (:408-417). Same Client/PurchaseItem spend+grant --
	// PlayFab is the anti-spoof authority (rejects insufficient funds / price mismatch).
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online)
	{
		OnComplete.ExecuteIfBound(false);
		return;
	}
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("ItemId"), CosmeticId.ToString());
	Body->SetStringField(TEXT("VirtualCurrency"), CurrencyCode);
	Body->SetNumberField(TEXT("Price"), Price);
	Body->SetStringField(TEXT("CatalogVersion"), TEXT("AFL_Main"));
	Online->PostClientApi(TEXT("PurchaseItem"), Body,
		[OnComplete](bool bOk, TSharedPtr<FJsonObject> /*Data*/)
		{
			OnComplete.ExecuteIfBound(bOk);
		}, /*bRequireAuth*/ true);
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

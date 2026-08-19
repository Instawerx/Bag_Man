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

// --- CC-3.3 COUNTED ENTITLEMENT -------------------------------------------------------------------
// Mirrors the OwnedSet pair exactly (RecordFor -> assign -> Flush; cache read fires the delegate with
// bOk=false on a miss) so the counted shape has the same failure modes as the proven boolean one.
// DELIBERATELY CACHE-ONLY: no PlayFab branch, unlike LoadOwnedSet. The remote blob is CC-3.4 in the
// separate Bag_Man_Backend repo, and inventing a transport here would be a backend write this lane
// does not own. A local round-trip is real and testable; a fabricated remote one is not.

void UAFLEconomyPersistenceSubsystem::LoadCountedSet(const FAFLPlayerId& Player, FAFLOnCountedSetLoaded OnLoaded)
{
	EnsureLoaded();
	if (const FAFLEconomyRecord* Rec = SaveData->Records.Find(ResolveKey(Player)))
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadCountedSet cache HIT keys=%d"), Rec->CountedEntitlements.Num());
		OnLoaded.ExecuteIfBound(true, Rec->CountedEntitlements);
	}
	else
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadCountedSet cache MISS (new player)"));
		OnLoaded.ExecuteIfBound(false, FAFLCountedEntitlementMap());
	}
}

void UAFLEconomyPersistenceSubsystem::SaveCountedSet(const FAFLPlayerId& Player, const FAFLCountedEntitlementMap& Counts)
{
	FAFLEconomyRecord& Rec = RecordFor(Player);
	// PRUNE ZEROES. A key at 0 and a key that was never granted must be indistinguishable, or the blob
	// grows a history of spent entitlements that later reads could mistake for a grant.
	Rec.CountedEntitlements.Reset();
	int32 Pruned = 0;
	for (const TPair<FName, int32>& KV : Counts)
	{
		if (KV.Value > 0) { Rec.CountedEntitlements.Add(KV.Key, KV.Value); }
		else { ++Pruned; }
	}
	UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] SaveCountedSet keys=%d pruned=%d"),
		Rec.CountedEntitlements.Num(), Pruned);
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
		// Every run states what it INHERITED before anything is touched -- a restored overlay makes an identical
		// set a no-op (no delta -> no OnRep), which cost a full session to surface once already.
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadSelection HIT (bHasSelection=%d) INHERITED-CREATOR: bUse=%d Body=(%.4f,%.4f,%.4f) Edge=(%.4f,%.4f,%.4f) Glow=(%.4f,%.4f,%.4f)"),
			Rec->bHasSelection ? 1 : 0, (int32)Rec->Selection.bUseCreatorColors,
			Rec->Selection.CreatorBodyColor.R, Rec->Selection.CreatorBodyColor.G, Rec->Selection.CreatorBodyColor.B,
			Rec->Selection.CreatorEdgeColor.R, Rec->Selection.CreatorEdgeColor.G, Rec->Selection.CreatorEdgeColor.B,
			Rec->Selection.CreatorGlowColor.R, Rec->Selection.CreatorGlowColor.G, Rec->Selection.CreatorGlowColor.B);
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

// --- CC-4.1 CONDITIONAL ENTITLEMENT ---------------------------------------------------------------
// Mirrors the CountedSet pair. DELIBERATE DIVERGENCE: zero-pruning does NOT apply here. A Lapsed
// grant is meaningful state -- the CC-4.2 lapse rule must be able to tell a lapsed subscriber from
// someone who never subscribed, and dropping the entry would erase exactly that distinction.

void UAFLEconomyPersistenceSubsystem::LoadConditionalSet(const FAFLPlayerId& Player, FAFLOnConditionalSetLoaded OnLoaded)
{
	EnsureLoaded();
	if (const FAFLEconomyRecord* Rec = SaveData->Records.Find(ResolveKey(Player)))
	{
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadConditionalSet cache HIT conditions=%d"), Rec->ConditionalGrants.Num());
		OnLoaded.ExecuteIfBound(true, Rec->ConditionalGrants);
	}
	else
	{
		// bOk=false means NEW PLAYER, not "lapsed". The caller must leave every condition Unknown --
		// treating a miss as Lapsed would apply the penalty path to someone who never had anything.
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadConditionalSet cache MISS (new player)"));
		OnLoaded.ExecuteIfBound(false, FAFLConditionalGrantMap());
	}
}

void UAFLEconomyPersistenceSubsystem::SaveConditionalSet(const FAFLPlayerId& Player, const FAFLConditionalGrantMap& Grants)
{
	FAFLEconomyRecord& Rec = RecordFor(Player);
	Rec.ConditionalGrants = Grants;
	int32 Held = 0, Lapsed = 0, Unknown = 0;
	for (const TPair<FName, FAFLConditionalGrant>& KV : Grants)
	{
		switch (KV.Value.State)
		{
		case EAFLConditionState::Held:   ++Held;   break;
		case EAFLConditionState::Lapsed: ++Lapsed; break;
		default:                         ++Unknown; break;
		}
	}
	UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] SaveConditionalSet conditions=%d held=%d lapsed=%d unknown=%d"),
		Grants.Num(), Held, Lapsed, Unknown);
	Flush();
}

// --- CC-3.5 SAVED BUILDS THROUGH THE BACKEND ------------------------------------------------------
// REMOTE-FIRST, CACHE-FALLBACK -- the same shape LoadOwnedSet already uses for PlayFab, so builds fail
// the way the proven path fails. If the signer is not configured (not a dedicated server, or the URL
// env is unset) PostServerCreatorBuilds skips and reports failure, and we fall back to cache rather
// than telling the player they have no robots.

void UAFLEconomyPersistenceSubsystem::LoadCreatorBuilds(const FAFLPlayerId& Player, const FString& PlayFabId, FAFLOnCreatorBuildsLoaded OnLoaded)
{
	EnsureLoaded();
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (Online && !PlayFabId.IsEmpty())
	{
		const int64 Ts = FDateTime::UtcNow().ToUnixTimestamp();
		const FString Body = FString::Printf(
			TEXT("{\"playFabId\":\"%s\",\"op\":\"load\",\"ts\":%lld}"),
			*PlayFabId, static_cast<long long>(Ts));
		TWeakObjectPtr<UAFLEconomyPersistenceSubsystem> WeakThis(this);
		Online->PostServerCreatorBuilds(Body, [WeakThis, Player, OnLoaded](bool bOk, const FString& Resp)
		{
			UAFLEconomyPersistenceSubsystem* Self = WeakThis.Get();
			if (!Self) { return; }
			if (bOk)
			{
				// The handler answers found:false for a NEW PLAYER with HTTP 200. That is not a failure and
				// must not fall back to cache -- the remote authoritatively said "no builds yet".
				const bool bFound = Resp.Contains(TEXT("\"found\":true"));
				UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadCreatorBuilds remote OK found=%d bytes=%d"),
					bFound ? 1 : 0, Resp.Len());
				if (bFound) { Self->CacheBuildsFor(Player, Resp); }
				OnLoaded.ExecuteIfBound(bFound, Resp);
				return;
			}
			UE_LOG(LogAFLEconPersist, Warning, TEXT("[EconPersist] LoadCreatorBuilds remote FAILED -> cache"));
			Self->ReadBuildsFromCache(Player, OnLoaded);
		});
		return;
	}
	ReadBuildsFromCache(Player, OnLoaded);
}

void UAFLEconomyPersistenceSubsystem::ReadBuildsFromCache(const FAFLPlayerId& Player, FAFLOnCreatorBuildsLoaded OnLoaded)
{
	EnsureLoaded();
	if (const FAFLEconomyRecord* Rec = SaveData->Records.Find(ResolveKey(Player)))
	{
		const bool bHas = !Rec->CreatorBuildsJson.IsEmpty();
		UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadCreatorBuilds cache %s"), bHas ? TEXT("HIT") : TEXT("EMPTY"));
		OnLoaded.ExecuteIfBound(bHas, Rec->CreatorBuildsJson);
		return;
	}
	UE_LOG(LogAFLEconPersist, Log, TEXT("[EconPersist] LoadCreatorBuilds cache MISS (new player)"));
	OnLoaded.ExecuteIfBound(false, FString());
}

void UAFLEconomyPersistenceSubsystem::CacheBuildsFor(const FAFLPlayerId& Player, const FString& BuildsJson)
{
	FAFLEconomyRecord& Rec = RecordFor(Player);
	Rec.CreatorBuildsJson = BuildsJson;
	Flush();
}

void UAFLEconomyPersistenceSubsystem::SaveCreatorBuilds(const FAFLPlayerId& Player, const FString& PlayFabId, const FString& BuildsJson, int32 Rev)
{
	// Cache FIRST, then push. If the remote call fails the player still has their robots locally, which
	// is the whole reason the cache exists -- ordering it the other way would lose the save on a timeout.
	CacheBuildsFor(Player, BuildsJson);

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online || PlayFabId.IsEmpty()) { return; }

	const int64 Ts = FDateTime::UtcNow().ToUnixTimestamp();
	const FString Body = FString::Printf(
		TEXT("{\"playFabId\":\"%s\",\"op\":\"save\",\"ts\":%lld,\"rev\":%d,\"builds\":%s}"),
		*PlayFabId, static_cast<long long>(Ts), Rev, *BuildsJson);
	Online->PostServerCreatorBuilds(Body, [](bool bOk, const FString& Resp)
	{
		// A 409 superseded is a CORRECT rejection of a stale write, not a failure -- report it distinctly
		// so a race that the guard handled cannot be mistaken for the store being down.
		UE_LOG(LogAFLEconPersist, Log, TEXT("AFL_TEST[BUILDSYNC] save remote ok=%d resp=%s"),
			bOk ? 1 : 0, *Resp.Left(140));
	});
}


// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLWalletComponent.h"

#include "Cosmetics/AFLEconomyPersistenceSubsystem.h"  // Phase A0: local SaveGame persistence -- the GetPersistence() swap point
#include "AFLOnlineSubsystem.h"                         // A1.1: PlayFabId = the durable account key for MakePlayerId
#include "Dom/JsonObject.h"                             // A1.2: PurchaseItem body + GetUserInventory parse (verify)
#include "AFLCosmeticCatalogSubsystem.h"            // catalog price/tier lookup for the purchase path (AFLCosmeticCore)
#include "AFLCosmeticCoreTypes.h"                   // FAFLCatalogEntry, EAFLAcquisition, EAFLCosmeticTier
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Player/LyraPlayerState.h"
// A1.3 step-3 earn hook -- compose the proven, committed accessors (identity/matchId) + the transport.
#include "Cosmetics/AFLPlayerIdentityComponent.h"   // GetResolvedPlayFabId (A1.4)
#include "Round/AFLRoundManagerComponent.h"          // GetMatchId (A1.3b)
#include "Engine/World.h"                            // GetWorld()->GetGameState()
#include "GameFramework/GameStateBase.h"             // AGameStateBase::FindComponentByClass
#include "Misc/CoreMisc.h"                           // IsRunningDedicatedServer()
#include "Misc/DateTime.h"                           // contract ts
#include "Misc/Guid.h"                               // nonce
#include "Serialization/JsonReader.h"                // parse newBalance from the earn response
#include "Serialization/JsonSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLWalletComponent)

DEFINE_LOG_CATEGORY_STATIC(LogAFLWalletDiag, Log, All);

// Per-layer diagnostic cvar (OFF by default), the SkinDiag pattern -- instrument = infrastructure. Makes a
// composite wallet failure diagnosable per seam (balance / gate / earn-spend) without bisecting.
static int32 GAFLWalletDiag = 0;
static FAutoConsoleVariableRef CVarAFLWalletDiag(
	TEXT("afl.WalletDiag"),
	GAFLWalletDiag,
	TEXT("AFL wallet per-layer diagnostics (0=off, 1=on): balance replication, entitlement gate verdicts, earn/spend validation + before/after."),
	ECVF_Default);

namespace
{
	bool WalletDiagOn() { return GAFLWalletDiag != 0; }

	// "[Wallet][SRV|CLI][f=<frame>] " -- mirrors AFLSkinDiag::Prefix so the two diags read alike in one log.
	FString WalletPrefix(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const TCHAR* Side = TEXT("---");
		if (World)
		{
			const ENetMode NM = World->GetNetMode();
			Side = (NM == NM_Client) ? TEXT("CLI") : TEXT("SRV");
		}
		return FString::Printf(TEXT("[Wallet][%s][f=%llu] "), Side, (unsigned long long)GFrameCounter);
	}

	// CLIENT-WALLET-REFRESH SPLITTING CHECK (per-client seam, the #43-race family). The bare [CLI] tag is
	// NOT PIE-world-isolated (both client worlds share the log; PlayerState indices don't correspond across
	// worlds), so a [CLI] line alone can't tell WHICH client's wallet ran OnRep. This keys on IDENTITY:
	//   localPS=y  -> the owning PlayerState belongs to THIS world's LOCAL player (i.e. Client 1's own wallet,
	//                 the instance whose store we are watching) -- NOT a simulated proxy of another player.
	//   role       -> the owner's local net role (Authority / AutonomousProxy / SimulatedProxy).
	//   boundUI=N  -> how many listeners are bound to OnWalletChanged on THIS instance right now. This is the
	//                 SPLIT: if OnRep fires on the local client's wallet with boundUI=0, the store never bound
	//                 (opened/resolved before the wallet, or bound a different instance) = UI-refresh gap; if
	//                 boundUI>0 yet the store is still wrong, the refresh logic itself is at fault; if OnRep
	//                 never fires on the local client at all = replication gap.
	FString WalletOwnerCtx(const UAFLWalletComponent* Comp)
	{
		const AActor* Owner = Comp ? Comp->GetOwner() : nullptr;
		const APlayerState* PS = Cast<APlayerState>(Owner);
		bool bLocalPS = false;
		if (PS)
		{
			// The owning PlayerState is the local player's iff its controller is the local controller in
			// this world (server-side this is true for the host's own PS; on a client it's true only for
			// that client's own PS -- exactly the disambiguation we need).
			if (const APlayerController* PC = PS->GetPlayerController())
			{
				bLocalPS = PC->IsLocalController();
			}
		}
		const TCHAR* Role = TEXT("?");
		if (Owner)
		{
			switch (Owner->GetLocalRole())
			{
			case ROLE_Authority:       Role = TEXT("Authority");      break;
			case ROLE_AutonomousProxy: Role = TEXT("AutonomousProxy");break;
			case ROLE_SimulatedProxy:  Role = TEXT("SimulatedProxy"); break;
			default:                   Role = TEXT("None");           break;
			}
		}
		return FString::Printf(TEXT("localPS=%s role=%s boundUI=%d"),
			bLocalPS ? TEXT("y") : TEXT("n"), Role, Comp ? Comp->GetOnWalletChangedBoundCount() : -1);
	}
}

UAFLWalletComponent::UAFLWalletComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// UGameFrameworkComponent has no replicated base -> WE enable replication or Volts/Watts/OwnedSet never
	// reach clients (the "compiles but doesn't replicate" trap the loadout documents). Wallet mutations are
	// purchase/earn-rare -> no tick.
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLWalletComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLWalletComponent, Volts);
	DOREPLIFETIME(UAFLWalletComponent, Watts);
	DOREPLIFETIME(UAFLWalletComponent, OwnedCosmeticIds);
}

void UAFLWalletComponent::BeginPlay()
{
	Super::BeginPlay();

	if (WalletDiagOn())
	{
		UE_LOG(LogAFLWalletDiag, Log, TEXT("%s[a] BeginPlay on %s (authority=%s) volts=%d watts=%d owned=%d"),
			*WalletPrefix(this), GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			(GetOwner() && GetOwner()->HasAuthority()) ? TEXT("y") : TEXT("n"),
			Volts, Watts, OwnedCosmeticIds.Num());
	}

	// Authority loads the player's economic state (balance + owned set) from persistence; seeds defaults if new.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		LoadFromPersistence();
	}
}

// =====================================================================================================
// (b) ENTITLEMENT GATE -- this component IS the real IAFLEntitlementSource (replaces the permissive stub).
// =====================================================================================================
bool UAFLWalletComponent::IsEntitled(const ALyraPlayerState* /*Player*/, FName CosmeticId) const
{
	if (CosmeticId == NAME_None)
	{
		return true; // "no cosmetic requested for this axis" -> not a gate failure.
	}

	// GrantedFree cosmetics (identity / free base / basic colors) are owned by everyone -- the catalog says so.
	bool bGrantedFree = false;
	if (const UAFLCosmeticCatalogSubsystem* Catalog = GetCatalog())
	{
		if (const FAFLCatalogEntry* Entry = Catalog->FindEntry(CosmeticId))
		{
			bGrantedFree = (Entry->Acquisition == EAFLAcquisition::GrantedFree);
		}
	}

	const bool bOwned = bGrantedFree || OwnedCosmeticIds.Contains(CosmeticId);

	if (WalletDiagOn())
	{
		UE_LOG(LogAFLWalletDiag, Log, TEXT("%s[b] IsEntitled(%s) = %s (grantedFree=%s ownedSet=%s)"),
			*WalletPrefix(this), *CosmeticId.ToString(), bOwned ? TEXT("Y") : TEXT("N"),
			bGrantedFree ? TEXT("y") : TEXT("n"), OwnedCosmeticIds.Contains(CosmeticId) ? TEXT("y") : TEXT("n"));
	}
	return bOwned;
}

bool UAFLWalletComponent::OwnsIdentity(const ALyraPlayerState* /*Player*/, EAFLIdentityType /*Type*/, FName Id) const
{
	// Identity (Team/Character) -- founding teams + the free Character base are GrantedFree; treat the same
	// as IsEntitled (catalog GrantedFree OR explicitly owned). Same gate, identity flavor.
	if (Id == NAME_None)
	{
		return true;
	}
	bool bGrantedFree = false;
	if (const UAFLCosmeticCatalogSubsystem* Catalog = GetCatalog())
	{
		if (const FAFLCatalogEntry* Entry = Catalog->FindEntry(Id))
		{
			bGrantedFree = (Entry->Acquisition == EAFLAcquisition::GrantedFree);
		}
	}
	const bool bOwned = bGrantedFree || OwnedCosmeticIds.Contains(Id);
	if (WalletDiagOn())
	{
		UE_LOG(LogAFLWalletDiag, Log, TEXT("%s[b] OwnsIdentity(%s) = %s (grantedFree=%s)"),
			*WalletPrefix(this), *Id.ToString(), bOwned ? TEXT("Y") : TEXT("N"), bGrantedFree ? TEXT("y") : TEXT("n"));
	}
	return bOwned;
}

// =====================================================================================================
// (c) EARN / SPEND -- server-authoritative mutations, all funneled through CommitMutation.
// =====================================================================================================
void UAFLWalletComponent::ServerEarnWatts_Implementation(int32 Amount)
{
	// A1.3a EARN-FORGE CLOSURE (mirror of ServerPurchaseCosmetic's dev-only shipping guard below): this
	// client-callable Server RPC (Validate=true; clamps only negatives) would let a shipping client forge
	// arbitrary positive currency. The ONLY legitimate earn is authority-only EarnWattsAuthority (extraction);
	// this RPC exists solely for the afl.Wallet.Earn dev cheat -> inert in shipping.
#if UE_BUILD_SHIPPING
	UE_LOG(LogAFLWalletDiag, Warning, TEXT("%s ServerEarnWatts is DEV-ONLY; ignored in shipping (legit earn = authority-only EarnWattsAuthority)."), *WalletPrefix(this));
	return;
#else
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	const int32 Clamped = FMath::Max(0, Amount); // server-validated: no negative earn.
	CommitMutation(/*dVolts*/0, /*dWatts*/Clamped, /*grant*/NAME_None, TEXT("EarnWatts"));
#endif
}

void UAFLWalletComponent::ServerEarnVolts_Implementation(int32 Amount)
{
	// A1.3a: same dev-only shipping guard as ServerEarnWatts (no client-forged Volts in shipping).
#if UE_BUILD_SHIPPING
	UE_LOG(LogAFLWalletDiag, Warning, TEXT("%s ServerEarnVolts is DEV-ONLY; ignored in shipping."), *WalletPrefix(this));
	return;
#else
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	const int32 Clamped = FMath::Max(0, Amount);
	CommitMutation(Clamped, 0, NAME_None, TEXT("EarnVolts"));
#endif
}

void UAFLWalletComponent::EarnWattsAuthority(int32 Amount, const TCHAR* Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	CommitMutation(/*dVolts*/0, /*dWatts*/FMath::Max(0, Amount), /*grant*/NAME_None, Reason);

	// A1.3 step 3 -- THE EARN HOOK: mirror the server-authoritative committed Watts delta to the player's PlayFab
	// wallet via the /earn Lambda. Runs AFTER CommitMutation above (the LOCAL wallet has already banked + persisted;
	// PlayFab mirrors the authoritative delta). Every gate below is a FAIL-SAFE SKIP (log + return) -- never a bad
	// grant. Composes the proven, committed accessors (identity/matchId/transport); modifies none of them.

	// (a) ANTI-SPOOF GATE: push ONLY on the trusted dedicated server. A listen-host is an untrusted client-authority
	// and must NOT push (NO || GIsEditor -- that was canary-only; a real PIE extraction must not grant real currency).
	if (!IsRunningDedicatedServer())
	{
		UE_LOG(LogAFLWalletDiag, Log, TEXT("%sAFL_A13S3 skip: not dedicated server"), *WalletPrefix(this));
		return;
	}
	// (b) the committed delta (the contract requires amount > 0).
	const int32 PushAmount = FMath::Max(0, Amount);
	if (PushAmount == 0)
	{
		UE_LOG(LogAFLWalletDiag, Log, TEXT("%sAFL_A13S3 skip: zero amount"), *WalletPrefix(this));
		return;
	}
	// (c) the earning player's server-VERIFIED PlayFabId (A1.4). Empty -> skip: never push with an empty id (the
	// safe failure that avoids misgranting).
	const UAFLPlayerIdentityComponent* Identity = GetOwner()->FindComponentByClass<UAFLPlayerIdentityComponent>();
	const FString PlayFabId = Identity ? Identity->GetResolvedPlayFabId() : FString();
	if (PlayFabId.IsEmpty())
	{
		UE_LOG(LogAFLWalletDiag, Warning, TEXT("%sAFL_A13S3 skip: no resolved PlayFabId (identity unresolved)"), *WalletPrefix(this));
		return;
	}
	// (d) the match this earn belongs to (A1.3b). Absent (non-Arena / no round manager) -> skip (decision #1: no
	// non-Arena push; the contract requires a non-empty matchId).
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	const UAFLRoundManagerComponent* Round = GS ? GS->FindComponentByClass<UAFLRoundManagerComponent>() : nullptr;
	const FString MatchId = Round ? Round->GetMatchId() : FString();
	if (MatchId.IsEmpty())
	{
		UE_LOG(LogAFLWalletDiag, Warning, TEXT("%sAFL_A13S3 skip: no matchId (no RoundManager / non-Arena)"), *WalletPrefix(this));
		return;
	}
	// (e) route the earn transaction through the persistence seam (Phase 1 consolidation: the Nonce/Ts/body build
	// + the /earn transport were formerly INLINE here; they now live in UAFLEconomyPersistenceSubsystem behind
	// IAFLCosmeticPersistence, at parity with the already-seamed load side). The COMPLETION stays here -- the
	// AFL_A13S3 grant log is the earn proof marker and fires identically. Null seam -> no push (same fail-safe as
	// the old null-Online guard). PlayFabId (A1.4 resolved id) + MatchId + PushAmount are already resolved above.
	if (IAFLCosmeticPersistence* Persistence = GetPersistence())
	{
		Persistence->EarnThroughBackend(PlayFabId, TEXT("WA"), PushAmount, FString(Reason ? Reason : TEXT("")), MatchId,
			FAFLOnEarnComplete::CreateLambda([PlayFabId, PushAmount](bool bOk, const FString& Resp)
			{
				if (bOk)
				{
					int32 NewBal = -1;
					TSharedPtr<FJsonObject> Root;
					const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp);
					if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
					{
						Root->TryGetNumberField(TEXT("newBalance"), NewBal);
					}
					UE_LOG(LogAFLWalletDiag, Log, TEXT("AFL_A13S3 earn ok pid=%s +%d newBal=%d"), *PlayFabId, PushAmount, NewBal);
				}
				else
				{
					UE_LOG(LogAFLWalletDiag, Warning, TEXT("AFL_A13S3 earn FAIL %s"), *Resp.Left(300));
				}
			}));
	}
}

void UAFLWalletComponent::ServerPurchaseCosmetic_Implementation(FName CosmeticId, EAFLPayCurrency PayWith)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }

	// A1.2: DEV-ONLY / advisory. The real, anti-spoof purchase is ClientRequestPurchase -> PlayFab (the
	// listen-host is untrusted and cannot be the economy authority). This local-deduct path stays for the
	// dev cheat (afl.Wallet.Buy) but is COMPILED OUT OF SHIPPING so it can never be a spend/grant bypass.
#if UE_BUILD_SHIPPING
	UE_LOG(LogAFLWalletDiag, Warning, TEXT("%s ServerPurchaseCosmetic is DEV-ONLY; shipping purchase = ClientRequestPurchase (PlayFab)."), *WalletPrefix(this));
	return;
#else

	const FString Pre = FString::Printf(TEXT("volts=%d watts=%d"), Volts, Watts);
	auto Deny = [&](const TCHAR* Why)
	{
		if (WalletDiagOn())
		{
			UE_LOG(LogAFLWalletDiag, Log, TEXT("%s[c] Purchase(%s) DENIED: %s (%s)"),
				*WalletPrefix(this), *CosmeticId.ToString(), Why, *Pre);
		}
	};

	const UAFLCosmeticCatalogSubsystem* Catalog = GetCatalog();
	const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(CosmeticId) : nullptr;
	if (!Entry) { Deny(TEXT("not in catalog")); return; }
	if (!Entry->bTransactable) { Deny(TEXT("not yet available (backend-gated)")); return; } // B1 INERT GATE: author-now-inert SKU cannot transact until B2 de-inerts it.
	if (Entry->Acquisition == EAFLAcquisition::GrantedFree) { Deny(TEXT("GrantedFree (already owned by all)")); return; }
	if (OwnedCosmeticIds.Contains(CosmeticId)) { Deny(TEXT("already owned (no double charge)")); return; }

	// --- Cost model (IRONICS LOCKED) -----------------------------------------------------------------
	// SPARK (accessible) is payable in EITHER Volts OR Watts; SURGE/ARC/THUNDERBOLT are Volts-only. The
	// catalog carries whichever price(s) apply (a SPARK item has BOTH PriceVolts and PriceWatts set; a
	// Volts-only tier has PriceWatts==0). The STORE passes PayWith=Volts/Watts for the player's chosen
	// path on a dual-priced item; PayWith=Auto (console cheat / single-arg callers / single-priced items)
	// lets the server pick: prefer Volts when affordable, else Watts.
	const bool bVoltsAvailable = (Entry->PriceVolts > 0);
	const bool bWattsAvailable = (Entry->PriceWatts > 0);
	if (!bVoltsAvailable && !bWattsAvailable) { Deny(TEXT("no price set (not purchasable)")); return; }

	// Resolve which currency to charge.
	bool bPayWatts;
	switch (PayWith)
	{
	case EAFLPayCurrency::Volts:
		if (!bVoltsAvailable) { Deny(TEXT("Volts payment requested but item has no Volts price")); return; }
		bPayWatts = false;
		break;
	case EAFLPayCurrency::Watts:
		if (!bWattsAvailable) { Deny(TEXT("Watts payment requested but item has no Watts price")); return; }
		bPayWatts = true;
		break;
	case EAFLPayCurrency::Auto:
	default:
		// Prefer Volts: pay Volts if the item has a Volts price AND the player can afford it; otherwise,
		// if it has a Watts price, fall back to Watts. (For a Volts-only item this is always Volts; for a
		// Watts-only item, always Watts; for a dual-priced SPARK item, Volts-first.)
		bPayWatts = bVoltsAvailable ? (Volts < Entry->PriceVolts && bWattsAvailable) : bWattsAvailable;
		break;
	}

	const int32 CostVolts = bPayWatts ? 0 : Entry->PriceVolts;
	const int32 CostWatts = bPayWatts ? Entry->PriceWatts : 0;

	if (Volts < CostVolts) { Deny(TEXT("insufficient Volts")); return; }
	if (Watts < CostWatts) { Deny(TEXT("insufficient Watts")); return; }

	// Commit: deduct + grant ownership in one funnel.
	CommitMutation(-CostVolts, -CostWatts, CosmeticId, TEXT("Purchase"));
#endif
}

//~ A1.2 -- PlayFab-native purchase (the anti-spoof path; replaces ServerPurchaseCosmetic for shipping) ---

void UAFLWalletComponent::ClientRequestPurchase(FName CosmeticId, EAFLPayCurrency PayWith)
{
	ClientRequestPurchase(CosmeticId, PayWith, TFunction<void(bool)>());
}

void UAFLWalletComponent::ClientRequestPurchase(FName CosmeticId, EAFLPayCurrency PayWith, TFunction<void(bool)> OnComplete)
{
	auto Fail = [&OnComplete](const TCHAR* Why)
	{
		UE_LOG(LogAFLWalletDiag, Log, TEXT("[Wallet] ClientRequestPurchase denied: %s"), Why);
		if (OnComplete) { OnComplete(false); }
	};

	const UAFLCosmeticCatalogSubsystem* Catalog = GetCatalog();
	const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(CosmeticId) : nullptr;
	if (!Entry) { Fail(TEXT("not in catalog")); return; }
	if (!Entry->bTransactable) { Fail(TEXT("not yet available (backend-gated)")); return; } // B1 INERT GATE: author-now-inert SKU cannot transact until B2 de-inerts it.
	if (Entry->Acquisition == EAFLAcquisition::GrantedFree) { Fail(TEXT("GrantedFree (no price)")); return; }
	// NOTE: no local already-owned guard -- PlayFab is the authority (it rejects a non-stackable double-buy,
	// allows a stackable re-buy). The store greys out owned items for DISPLAY only, never as the gate.

	const bool bVoltsAvailable = (Entry->PriceVolts > 0);
	const bool bWattsAvailable = (Entry->PriceWatts > 0);
	if (!bVoltsAvailable && !bWattsAvailable) { Fail(TEXT("no price set")); return; }
	bool bPayWatts;
	switch (PayWith)
	{
	case EAFLPayCurrency::Volts: if (!bVoltsAvailable) { Fail(TEXT("no Volts price")); return; } bPayWatts = false; break;
	case EAFLPayCurrency::Watts: if (!bWattsAvailable) { Fail(TEXT("no Watts price")); return; } bPayWatts = true;  break;
	default:                     bPayWatts = !bVoltsAvailable; break; // Auto: prefer Volts; PlayFab enforces funds.
	}
	const FString VC = bPayWatts ? TEXT("WA") : TEXT("VO");
	const int32 Price = bPayWatts ? Entry->PriceWatts : Entry->PriceVolts;

	IAFLCosmeticPersistence* Persistence = GetPersistence();
	if (!Persistence) { Fail(TEXT("persistence seam unavailable")); return; }

	const int32 CostV = bPayWatts ? 0 : Price;
	const int32 CostW = bPayWatts ? Price : 0;
	TWeakObjectPtr<UAFLWalletComponent> WeakThis(this);
	// Phase 1 consolidation: the body build + Client/PurchaseItem transport were formerly INLINE here; they now
	// live in UAFLEconomyPersistenceSubsystem behind IAFLCosmeticPersistence. The COMPLETION stays here -- the
	// REJECTED log + ApplyPurchaseResult (the Option-A local mirror + owned re-read) fire identically.
	Persistence->PurchaseThroughBackend(CosmeticId, VC, Price,
		FAFLOnPurchaseComplete::CreateLambda([WeakThis, CosmeticId, CostV, CostW, OnComplete](bool bOk)
		{
			UAFLWalletComponent* Self = WeakThis.Get();
			if (!Self) { if (OnComplete) { OnComplete(false); } return; }
			if (!bOk)
			{
				// PlayFab REJECTED (insufficient PlayFab funds / price mismatch) -- the anti-spoof wall.
				UE_LOG(LogAFLWalletDiag, Log, TEXT("[Wallet] PurchaseItem(%s) REJECTED by PlayFab (funds/price)."), *CosmeticId.ToString());
				if (OnComplete) { OnComplete(false); }
				return;
			}
			Self->ApplyPurchaseResult(CosmeticId, CostV, CostW, OnComplete);
		}));
}

void UAFLWalletComponent::ApplyPurchaseResult(FName CosmeticId, int32 CostVolts, int32 CostWatts, TFunction<void(bool)> OnComplete)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// BALANCE: mirror-deduct LOCALLY (display). Option A -- we do NOT overwrite the balance from PlayFab,
		// because that would wipe the proven local earn loop (extraction/loot, un-synced pre-A1.3).
		//
		// LABELED DISPLAY GAP (by design pre-A1.3 -- do NOT "fix" this locally):
		//   The local balance can OVER-DISPLAY vs the spendable PlayFab balance by the amount of un-synced
		//   earned Watts. This is EXPECTED and HARMLESS: the over-display is UN-SPENDABLE (PurchaseItem spends
		//   only PlayFab-held currency, server-enforced), so it is un-exploitable. It reconciles to PlayFab
		//   truth at A1.3, when earn routes to PlayFab. DO NOT patch it by writing the local balance as
		//   authoritative -- that reintroduces the spend spoof this layer closes.
		Volts = FMath::Max(0, Volts - CostVolts);
		Watts = FMath::Max(0, Watts - CostWatts);
	}

	IAFLCosmeticPersistence* Persistence = GetPersistence();
	if (!Persistence)
	{
		OnWalletChanged.Broadcast(Volts, Watts);
		if (OnComplete) { OnComplete(true); }
		return;
	}

	// OWNERSHIP: re-read from PlayFab (authoritative). REQ-2: the purchase is already server-committed, so a
	// failed re-read never loses it -- the owned-set display lags then reconciles from PlayFab on the next
	// load. NEVER a local-truth patch of ownership.
	TWeakObjectPtr<UAFLWalletComponent> WeakThis(this);
	Persistence->LoadOwnedSet(MakePlayerId(), FAFLOnOwnedSetLoaded::CreateLambda(
		[WeakThis, CosmeticId, OnComplete](bool bOk, const TArray<FName>& Owned)
		{
			UAFLWalletComponent* Self = WeakThis.Get();
			if (!Self) { if (OnComplete) { OnComplete(true); } return; }
			if (bOk && Self->GetOwner() && Self->GetOwner()->HasAuthority())
			{
				Self->OwnedCosmeticIds = Owned; // authoritative from PlayFab (now includes the purchase)
			}
			else if (!bOk)
			{
				UE_LOG(LogAFLWalletDiag, Log, TEXT("[Wallet] owned re-read failed post-purchase for %s (server-committed; display lags -> reconciles next load)."), *CosmeticId.ToString());
			}
			Self->OnWalletChanged.Broadcast(Self->Volts, Self->Watts);
			if (OnComplete) { OnComplete(true); }
		}));
}

//~ A1.2 verify harness helpers (afl.Online.VerifyA12) -----------------------------------------------
namespace
{
	static const int32 A12_TokenPrice = 10;        // must match AFL.Test.Token's VO price in the manifest
	static const int32 A12_PremiumPrice = 1000000; // AFL.Test.Premium's VO price (> the seeded balance)

	// Read PlayFab VO + the count of TokenIdStr instances from GetUserInventory.
	static void A12_ReadInventory(UAFLOnlineSubsystem* Online, FString TokenIdStr, TFunction<void(bool, int32, int32)> Cb)
	{
		if (!Online) { Cb(false, 0, 0); return; }
		Online->PostClientApi(TEXT("GetUserInventory"), MakeShared<FJsonObject>(),
			[Cb, TokenIdStr](bool bOk, TSharedPtr<FJsonObject> Data)
			{
				if (!bOk || !Data.IsValid()) { Cb(false, 0, 0); return; }
				int32 Vo = 0, Count = 0;
				const TSharedPtr<FJsonObject>* VC = nullptr;
				if (Data->TryGetObjectField(TEXT("VirtualCurrency"), VC) && VC) { (*VC)->TryGetNumberField(TEXT("VO"), Vo); }
				const TArray<TSharedPtr<FJsonValue>>* Inv = nullptr;
				if (Data->TryGetArrayField(TEXT("Inventory"), Inv) && Inv)
				{
					for (const TSharedPtr<FJsonValue>& It : *Inv)
					{
						const TSharedPtr<FJsonObject> Obj = It.IsValid() ? It->AsObject() : nullptr;
						FString Iid;
						if (Obj.IsValid() && Obj->TryGetStringField(TEXT("ItemId"), Iid) && Iid == TokenIdStr) { ++Count; }
					}
				}
				Cb(true, Vo, Count);
			}, /*bRequireAuth*/ true);
	}

	// Attempt a direct PurchaseItem at an arbitrary VO price. Cb(bAccepted) -- false = PlayFab rejected.
	static void A12_TryBuy(UAFLOnlineSubsystem* Online, FString ItemId, int32 Price, TFunction<void(bool)> Cb)
	{
		if (!Online) { Cb(false); return; }
		const TSharedRef<FJsonObject> B = MakeShared<FJsonObject>();
		B->SetStringField(TEXT("ItemId"), ItemId);
		B->SetStringField(TEXT("VirtualCurrency"), TEXT("VO"));
		B->SetNumberField(TEXT("Price"), Price);
		B->SetStringField(TEXT("CatalogVersion"), TEXT("AFL_Main"));
		Online->PostClientApi(TEXT("PurchaseItem"), B, [Cb](bool bOk, TSharedPtr<FJsonObject>) { Cb(bOk); }, /*bRequireAuth*/ true);
	}

	// Read PlayFab VO + the token's total UNIT count = sum over matching rows of max(1, RemainingUses). A
	// stackable re-buy increments RemainingUses on ONE row (no new row), so counting ROWS would miss the grant;
	// summing uses catches both the first grant (new row, >=1 unit) and a re-buy (uses+1). Used by the
	// production-seam verify to assert the grant robustly across re-runs (unlike A12's row-count, fragile on re-run).
	static void Seam_ReadTokenState(UAFLOnlineSubsystem* Online, FString TokenIdStr, TFunction<void(bool, int32, int32)> Cb)
	{
		if (!Online) { Cb(false, 0, 0); return; }
		Online->PostClientApi(TEXT("GetUserInventory"), MakeShared<FJsonObject>(),
			[Cb, TokenIdStr](bool bOk, TSharedPtr<FJsonObject> Data)
			{
				if (!bOk || !Data.IsValid()) { Cb(false, 0, 0); return; }
				int32 Vo = 0, Units = 0;
				const TSharedPtr<FJsonObject>* VC = nullptr;
				if (Data->TryGetObjectField(TEXT("VirtualCurrency"), VC) && VC) { (*VC)->TryGetNumberField(TEXT("VO"), Vo); }
				const TArray<TSharedPtr<FJsonValue>>* Inv = nullptr;
				if (Data->TryGetArrayField(TEXT("Inventory"), Inv) && Inv)
				{
					for (const TSharedPtr<FJsonValue>& It : *Inv)
					{
						const TSharedPtr<FJsonObject> Obj = It.IsValid() ? It->AsObject() : nullptr;
						FString Iid;
						if (Obj.IsValid() && Obj->TryGetStringField(TEXT("ItemId"), Iid) && Iid == TokenIdStr)
						{
							int32 Uses = 0;
							Units += (Obj->TryGetNumberField(TEXT("RemainingUses"), Uses) && Uses > 0) ? Uses : 1;
						}
					}
				}
				Cb(true, Vo, Units);
			}, /*bRequireAuth*/ true);
	}
}

void UAFLWalletComponent::DebugVerifyA12(FName TokenId, FName PremiumId, TFunction<void(const FAFLPurchaseVerifyResult&)> OnDone)
{
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online || !Online->IsLoggedIn())
	{
		FAFLPurchaseVerifyResult R;
		R.bLoginOk = (Online != nullptr) && Online->IsLoggedIn();
		R.FailNote = TEXT("not logged in");
		OnDone(R);
		return;
	}

	const FString TokenIdStr = TokenId.ToString();
	const FString PremiumIdStr = PremiumId.ToString();
	const int32 LocalVoBeforeBuy = Volts;

	TSharedRef<FAFLPurchaseVerifyResult> R = MakeShared<FAFLPurchaseVerifyResult>();
	R->bLoginOk = true;
	TWeakObjectPtr<UAFLWalletComponent> WeakThis(this);

	// 1) read PlayFab BEFORE -> 2) legit buy the token (real path) -> 3) read AFTER (server deducted+granted?)
	// -> 4) SPOOF fake-price (reject) -> 5) SPEND-SPOOF over-balance (reject; faked local UNSPENDABLE).
	A12_ReadInventory(Online, TokenIdStr,
		[WeakThis, R, TokenIdStr, PremiumIdStr, LocalVoBeforeBuy, OnDone](bool bOk1, int32 VoBefore, int32 CountBefore)
		{
			UAFLWalletComponent* Self = WeakThis.Get();
			if (!Self || !bOk1) { R->FailNote = TEXT("read-before failed"); OnDone(*R); return; }
			R->VoBefore = VoBefore;

			// Legit buy: call PlayFab PurchaseItem DIRECTLY (server deduct+grant) THEN ApplyPurchaseResult
			// (the game's real reflect path: mirror-deduct + owned re-read). Direct PurchaseItem -- not
			// ClientRequestPurchase -- so the re-runnable stackable TEST TOKEN need not live in the game's
			// COSMETIC catalog (test tokens aren't cosmetics); we still exercise ApplyPurchaseResult for the
			// mirror-deduct assertion. The store's real purchases DO go through ClientRequestPurchase (which
			// resolves a real cosmetic's price from FAFLCatalogEntry).
			A12_TryBuy(UAFLOnlineSubsystem::Get(Self), TokenIdStr, A12_TokenPrice,
				[WeakThis, R, TokenIdStr, PremiumIdStr, CountBefore, LocalVoBeforeBuy, OnDone](bool bBought)
				{
					UAFLWalletComponent* S2 = WeakThis.Get();
					if (!S2) { OnDone(*R); return; }

					// Continuation after the (attempted) legit buy: read AFTER -> spoof -> spend-spoof.
					auto AfterBuy = [WeakThis, R, TokenIdStr, PremiumIdStr, CountBefore, OnDone]()
					{
						UAFLWalletComponent* S3 = WeakThis.Get();
						A12_ReadInventory(S3 ? UAFLOnlineSubsystem::Get(S3) : nullptr, TokenIdStr,
							[WeakThis, R, TokenIdStr, PremiumIdStr, CountBefore, OnDone](bool bOk3, int32 VoAfter, int32 CountAfter)
							{
								if (bOk3) { R->VoAfter = VoAfter; R->bLegitOwnedOnPlayFab = (CountAfter > CountBefore); }
								UAFLWalletComponent* S4 = WeakThis.Get();
								A12_TryBuy(S4 ? UAFLOnlineSubsystem::Get(S4) : nullptr, TokenIdStr, 1,
									[WeakThis, R, PremiumIdStr, OnDone](bool bSpoofAccepted)
									{
										R->bSpoofRejected = !bSpoofAccepted;
										UAFLWalletComponent* S5 = WeakThis.Get();
										A12_TryBuy(S5 ? UAFLOnlineSubsystem::Get(S5) : nullptr, PremiumIdStr, A12_PremiumPrice,
											[R, OnDone](bool bSpendAccepted)
											{
												R->bSpendSpoofRejected = !bSpendAccepted;
												OnDone(*R);
											});
									});
							});
					};

					if (bBought)
					{
						// Reflect via the game's REAL path -> exercises the mirror-deduct + owned re-read.
						S2->ApplyPurchaseResult(FName(*TokenIdStr), A12_TokenPrice, 0,
							[WeakThis, R, LocalVoBeforeBuy, AfterBuy](bool)
							{
								UAFLWalletComponent* S2b = WeakThis.Get();
								R->bMirrorDeducted = (S2b != nullptr) && (S2b->GetVolts() == LocalVoBeforeBuy - A12_TokenPrice);
								AfterBuy();
							});
					}
					else
					{
						R->FailNote = TEXT("legit token buy rejected by PlayFab (VO seeded/enough?)");
						AfterBuy();
					}
				});
		});
}

void UAFLWalletComponent::DebugVerifyPurchaseSeam(TFunction<void(const FAFLPurchaseVerifyResult&)> OnDone)
{
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online || !Online->IsLoggedIn())
	{
		FAFLPurchaseVerifyResult R;
		R.bLoginOk = (Online != nullptr) && Online->IsLoggedIn();
		R.FailNote = TEXT("not logged in");
		OnDone(R);
		return;
	}

	// Make the PlayFab test token reachable by the PRODUCTION entry. ClientRequestPurchase resolves the price
	// from the cosmetic catalog (FindEntry), but the re-runnable token is deliberately not a shipping cosmetic --
	// and the PlayFab AFL_Main catalog seeds no shippable cosmetic (only the test items + one owned beam). Inject a
	// TRANSIENT catalog entry (in-memory, dev-only) mirroring the PlayFab price (10 VO) so the real entry resolves it.
	const FName TokenId(TEXT("AFL.Test.Token"));
	UAFLCosmeticCatalogSubsystem* Catalog = GetCatalog();
	if (!Catalog)
	{
		FAFLPurchaseVerifyResult R; R.bLoginOk = true; R.FailNote = TEXT("no cosmetic catalog subsystem"); OnDone(R); return;
	}
#if !UE_BUILD_SHIPPING
	{
		FAFLCatalogEntry TokenEntry;
		TokenEntry.CosmeticId  = TokenId;
		TokenEntry.Acquisition = EAFLAcquisition::Direct; // purchasable (NOT GrantedFree) -> ClientRequestPurchase proceeds
		TokenEntry.PriceVolts  = 10;                       // MUST match the PlayFab AFL_Main price (config/economy-catalog.json)
		TokenEntry.PriceWatts  = 0;
		Catalog->DebugInjectTransientEntry(TokenEntry);
	}
#endif

	const FString TokenIdStr = TokenId.ToString();
	const int32 LocalVoBeforeBuy = Volts;

	TSharedRef<FAFLPurchaseVerifyResult> R = MakeShared<FAFLPurchaseVerifyResult>();
	R->bLoginOk = true;
	TWeakObjectPtr<UAFLWalletComponent> WeakThis(this);

	// 1) read BEFORE -> 2) buy via the PRODUCTION entry (ClientRequestPurchase -> PurchaseThroughBackend, the
	//    Phase-1 relocated /PurchaseItem transport) -> 3) read AFTER (server deducted + granted?) -> 4) spend-spoof
	//    through the SAME entry (over-priced Premium) -> must be REJECTED (the relocated transport stays un-spoofable).
	Seam_ReadTokenState(Online, TokenIdStr,
		[WeakThis, R, TokenIdStr, LocalVoBeforeBuy, OnDone](bool bOk1, int32 VoBefore, int32 UnitsBefore)
		{
			UAFLWalletComponent* Self = WeakThis.Get();
			if (!Self || !bOk1) { R->FailNote = TEXT("read-before failed"); OnDone(*R); return; }
			R->VoBefore = VoBefore;

			// PRODUCTION buy through the real store entry -> FindEntry(transient) -> price 10 VO ->
			// PurchaseThroughBackend -> completion -> ApplyPurchaseResult (mirror-deduct + owned re-read).
			Self->ClientRequestPurchase(FName(*TokenIdStr), EAFLPayCurrency::Volts,
				[WeakThis, R, TokenIdStr, UnitsBefore, LocalVoBeforeBuy, OnDone](bool bAccepted)
				{
					UAFLWalletComponent* S2 = WeakThis.Get();
					if (!S2) { OnDone(*R); return; }
					R->bSeamAccepted   = bAccepted;
					// ApplyPurchaseResult ran inside the accept path -> the local mirror should now read (pre - 10).
					R->bMirrorDeducted = bAccepted && (S2->GetVolts() == LocalVoBeforeBuy - 10);

					Seam_ReadTokenState(UAFLOnlineSubsystem::Get(S2), TokenIdStr,
						[WeakThis, R, UnitsBefore, OnDone](bool bOk3, int32 VoAfter, int32 UnitsAfter)
						{
							if (bOk3) { R->VoAfter = VoAfter; R->bLegitOwnedOnPlayFab = (UnitsAfter > UnitsBefore); }

							UAFLWalletComponent* S4 = WeakThis.Get();
							if (!S4) { OnDone(*R); return; }

							// SPEND-SPOOF through the SAME production entry: fake local Volts HIGH, then buy the
							// over-priced Premium -> PlayFab must REJECT (PlayFab-held funds are what count; the faked
							// local balance is UNSPENDABLE -- ClientRequestPurchase never even consults local funds).
							S4->DebugSetBalance(9999999, 9999999);
							const FName PremiumId(TEXT("AFL.Test.Premium"));
#if !UE_BUILD_SHIPPING
							if (UAFLCosmeticCatalogSubsystem* Cat = S4->GetCatalog())
							{
								FAFLCatalogEntry PremiumEntry;
								PremiumEntry.CosmeticId  = PremiumId;
								PremiumEntry.Acquisition = EAFLAcquisition::Direct;
								PremiumEntry.PriceVolts  = 1000000; // matches PlayFab; >> the seeded balance -> InsufficientFunds
								PremiumEntry.PriceWatts  = 0;
								Cat->DebugInjectTransientEntry(PremiumEntry);
							}
#endif
							S4->ClientRequestPurchase(PremiumId, EAFLPayCurrency::Volts,
								[R, OnDone](bool bSpendAccepted)
								{
									R->bSpendSpoofRejected = !bSpendAccepted;
									OnDone(*R);
								});
						});
				});
		});
}

void UAFLWalletComponent::DebugSetBalance(int32 InVolts, int32 InWatts)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	// Direct authority set (seed/test) -- bypasses earn/spend; goes through the commit funnel for replicate+persist+diag.
	CommitMutation(InVolts - Volts, InWatts - Watts, NAME_None, TEXT("DebugSetBalance"));
}

void UAFLWalletComponent::DebugGrantOwnership(FName CosmeticId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || CosmeticId == NAME_None) { return; }
	CommitMutation(0, 0, CosmeticId, TEXT("DebugGrant"));
}

// The single authority commit point: apply the delta + optional grant, replicate (DOREPLIFETIME), persist,
// and diag the before/after. Listen-host applies locally here (OnRep does not fire on authority).
void UAFLWalletComponent::CommitMutation(int32 DeltaVolts, int32 DeltaWatts, FName GrantId, const TCHAR* Reason)
{
	const int32 PreV = Volts, PreW = Watts;

	Volts = FMath::Max(0, Volts + DeltaVolts); // never negative (peg floor).
	Watts = FMath::Max(0, Watts + DeltaWatts);

	bool bGranted = false;
	if (GrantId != NAME_None && !OwnedCosmeticIds.Contains(GrantId))
	{
		OwnedCosmeticIds.Add(GrantId);
		bGranted = true;
	}

	if (WalletDiagOn())
	{
		UE_LOG(LogAFLWalletDiag, Log, TEXT("%s[c] %s COMMIT: volts %d->%d watts %d->%d%s"),
			*WalletPrefix(this), Reason, PreV, Volts, PreW, Watts,
			bGranted ? *FString::Printf(TEXT(" + GRANTED %s (owned=%d)"), *GrantId.ToString(), OwnedCosmeticIds.Num()) : TEXT(""));
	}

	// Event-driven UI refresh on the AUTHORITY/listen-host (OnRep does not fire on authority). Remote clients
	// get it via OnRep_Balance/OnRep_OwnedSet below.
	OnWalletChanged.Broadcast(Volts, Watts);

	PersistState();
}

void UAFLWalletComponent::OnRep_Balance()
{
	// Remote clients: the balance replicated in -- the owner's HUD/store updates from here (event-driven).
	// SPLITTING CHECK: WalletOwnerCtx reports localPS (is this the local player's wallet, defeating the PIE
	// world-isolation trap) + boundUI (did the store's bind take on THIS instance). Read boundUI on the line
	// where localPS=y to split replication-gap (no line) from UI-refresh-gap (line present, boundUI=0).
	if (WalletDiagOn())
	{
		UE_LOG(LogAFLWalletDiag, Log, TEXT("%s[a] OnRep_Balance on %s (%s): volts=%d watts=%d"),
			*WalletPrefix(this), GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			*WalletOwnerCtx(this), Volts, Watts);
	}
	OnWalletChanged.Broadcast(Volts, Watts);
}

void UAFLWalletComponent::OnRep_OwnedSet()
{
	if (WalletDiagOn())
	{
		UE_LOG(LogAFLWalletDiag, Log, TEXT("%s[b] OnRep_OwnedSet on %s (%s): owned=%d"),
			*WalletPrefix(this), GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			*WalletOwnerCtx(this), OwnedCosmeticIds.Num());
	}
	// Ownership changed (a buy granted an item) -> refresh the store grid's owned badges. Carries the balance
	// too so a single binding updates both currency + ownership.
	OnWalletChanged.Broadcast(Volts, Watts);
}

// =====================================================================================================
// Plumbing -- player-state / persistence / catalog resolution (mirrors the loadout component).
// =====================================================================================================
ALyraPlayerState* UAFLWalletComponent::GetLyraPlayerState() const
{
	return GetPlayerState<ALyraPlayerState>();
}

IAFLCosmeticPersistence* UAFLWalletComponent::GetPersistence() const
{
	// Phase A0: the local SaveGame persistence subsystem -- the FIRST impl of the seam. Balance + owned-set
	// now SURVIVE a session boundary (buy -> restart -> still owned). Behind the SAME interface, A1 swaps this
	// for the Bag_Man_Backend Lambda tier (server-auth) with no call-site change. Null-tolerant: if the
	// subsystem isn't up yet, load/save no-op exactly as the stub did.
	return UAFLEconomyPersistenceSubsystem::Get(this);
}

FAFLPlayerId UAFLWalletComponent::MakePlayerId() const
{
	// A1.1: the durable account key is the PlayFabId (cross-session AND cross-device). Fall back to the
	// net-id (A0 behavior) when not logged in -- the persistence layer's ForceLocalSlot then applies.
	if (const UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this))
	{
		if (Online->IsLoggedIn() && !Online->GetPlayFabId().IsEmpty())
		{
			return FAFLPlayerId::MakeFromBacking(Online->GetPlayFabId());
		}
	}
	if (const APlayerState* PS = GetLyraPlayerState())
	{
		const FUniqueNetIdRepl& NetId = PS->GetUniqueId();
		if (NetId.IsValid())
		{
			return FAFLPlayerId::MakeFromBacking(NetId->ToString());
		}
	}
	return FAFLPlayerId();
}

UAFLCosmeticCatalogSubsystem* UAFLWalletComponent::GetCatalog() const
{
	return UAFLCosmeticCatalogSubsystem::Get(this);
}

void UAFLWalletComponent::LoadFromPersistence()
{
	IAFLCosmeticPersistence* Persistence = GetPersistence();
	if (!Persistence)
	{
		// No backend in bring-up: seed nothing (defaults 0/0/empty stay). The replicated UPROPERTYs hold the
		// session balance; the dev cheats seed test values. Diag notes the no-op.
		if (WalletDiagOn())
		{
			UE_LOG(LogAFLWalletDiag, Log, TEXT("%s[a] LoadFromPersistence: no backend (stub) -> session balance only"), *WalletPrefix(this));
		}
		return;
	}

	const FAFLPlayerId Id = MakePlayerId();
	TWeakObjectPtr<UAFLWalletComponent> WeakThis(this);
	Persistence->LoadBalance(Id, FAFLOnBalanceLoaded::CreateLambda([WeakThis](bool bFound, int32 InVolts, int32 InWatts)
	{
		if (UAFLWalletComponent* Self = WeakThis.Get())
		{
			if (Self->GetOwner() && Self->GetOwner()->HasAuthority())
			{
				Self->Volts = bFound ? InVolts : 0;
				Self->Watts = bFound ? InWatts : 0;
			}
		}
	}));
	Persistence->LoadOwnedSet(Id, FAFLOnOwnedSetLoaded::CreateLambda([WeakThis](bool bOk, const TArray<FName>& Owned)
	{
		if (UAFLWalletComponent* Self = WeakThis.Get())
		{
			if (bOk && Self->GetOwner() && Self->GetOwner()->HasAuthority())
			{
				Self->OwnedCosmeticIds = Owned;
			}
		}
	}));
}

void UAFLWalletComponent::PersistState() const
{
	if (IAFLCosmeticPersistence* Persistence = GetPersistence())
	{
		const FAFLPlayerId Id = MakePlayerId();
		Persistence->SaveBalance(Id, Volts, Watts);
		Persistence->SaveOwnedSet(Id, OwnedCosmeticIds);
	}
	// Stub: no-op (the replicated state IS the session source of truth).
}

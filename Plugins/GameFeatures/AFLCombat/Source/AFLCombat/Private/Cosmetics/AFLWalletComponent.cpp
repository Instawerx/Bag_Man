// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLWalletComponent.h"

#include "Cosmetics/AFLEconomyPersistenceSubsystem.h"  // Phase A0: local SaveGame persistence -- the GetPersistence() swap point
#include "AFLOnlineSubsystem.h"                         // A1.1: PlayFabId = the durable account key for MakePlayerId
#include "AFLCosmeticCatalogSubsystem.h"            // catalog price/tier lookup for the purchase path (AFLCosmeticCore)
#include "AFLCosmeticCoreTypes.h"                   // FAFLCatalogEntry, EAFLAcquisition, EAFLCosmeticTier
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Player/LyraPlayerState.h"

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
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	const int32 Clamped = FMath::Max(0, Amount); // server-validated: no negative earn.
	CommitMutation(/*dVolts*/0, /*dWatts*/Clamped, /*grant*/NAME_None, TEXT("EarnWatts"));
}

void UAFLWalletComponent::ServerEarnVolts_Implementation(int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	const int32 Clamped = FMath::Max(0, Amount);
	CommitMutation(Clamped, 0, NAME_None, TEXT("EarnVolts"));
}

void UAFLWalletComponent::EarnWattsAuthority(int32 Amount, const TCHAR* Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	CommitMutation(/*dVolts*/0, /*dWatts*/FMath::Max(0, Amount), /*grant*/NAME_None, Reason);
}

void UAFLWalletComponent::ServerPurchaseCosmetic_Implementation(FName CosmeticId, EAFLPayCurrency PayWith)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }

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

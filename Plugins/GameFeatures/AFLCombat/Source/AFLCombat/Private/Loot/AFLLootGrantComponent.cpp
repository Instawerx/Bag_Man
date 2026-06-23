// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootGrantComponent.h"

#include "AFLCombat.h"                          // LogAFLCombat
#include "Cosmetics/AFLWalletComponent.h"       // the Watts grant funnel (EarnWattsAuthority)
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Loot/AFLLootCarryComponent.h"         // the carried-at-risk pool (CarryToExtractEnergy -> Collect)
#include "Materials/MaterialInterface.h"        // PRESENTATION: ScatterGibMaterial (TObjectPtr UPROPERTY full type)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLootGrantComponent)

UAFLLootGrantComponent::UAFLLootGrantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLLootGrantComponent::Configure(EAFLLootValueModel InValueModel, int32 InValue,
	EAFLLootEligibility InEligibility, AActor* InOwnerActor, FName InGrantReason, UStaticMesh* InScatterGibMesh,
	EAFLBodyZone InOriginZone)
{
	ValueModel  = InValueModel;
	LootValue   = InValue;
	Eligibility = InEligibility;
	OwnerActor  = InOwnerActor;
	ScatterGibMesh = InScatterGibMesh;   // C3: null for caches (-> cube form); the dismember limb's own gib mesh otherwise
	ScatterZone = InOriginZone;          // V7-1: the part's specific zone (None for caches) -> the token's OriginZone
	if (!InGrantReason.IsNone())
	{
		GrantReason = InGrantReason.ToString();
	}
	// E2 cause-A (RACE fix -- causal ordering, NOT a timer): the loot is now CONFIGURED. The dismember head/limb
	// spawns ON the victim, so a pawn can overlap it BEFORE this runs (Initialize calls Configure right after
	// SpawnActor). Until this flag is set, OwnerActor/LootValue are unset -- the owner would auto-collect his own
	// head as a +0 "enemy" (the watched bug). Gating retrieval on bConfigured makes that IMPOSSIBLE regardless of
	// timing/lag (afl-cpp-lyra "Active vs Registered is real" -- registered-but-not-active until configured).
	bConfigured = true;
}

EAFLRetrievalMode UAFLLootGrantComponent::ResolveRetrievalMode(const AActor* Grabber) const
{
	// E2 cause-A (RACE fix): unconfigured loot is NOT retrievable by anyone -- Ineligible until Configure ran (so
	// the overlap/grab can never act with a null OwnerActor / 0 LootValue). State-based, race-proof (not a timer).
	if (!bConfigured)
	{
		return EAFLRetrievalMode::Ineligible;
	}
	// C3 -- the grab ability's PRE-mechanism query. Reuse the EXACT owner-vs-enemy resolution TryGrant uses
	// downstream (SSOT -- no duplicate match): owner (controller match) -> instant reattach; eligible non-owner
	// -> the collect-channel; else -> ineligible. No grant happens here; this only routes the mechanism.
	const AController* GrabberController = ResolveController(Grabber);
	if (OwnerActor.IsValid())
	{
		const AController* OwnerController = ResolveController(OwnerActor.Get());
		if (OwnerController != nullptr && GrabberController == OwnerController)
		{
			return EAFLRetrievalMode::OwnerReattach;
		}
	}
	return IsEligible(Grabber, GrabberController) ? EAFLRetrievalMode::EnemyCollect : EAFLRetrievalMode::Ineligible;
}

const AController* UAFLLootGrantComponent::ResolveController(const AActor* Actor)
{
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		return Pawn->GetController();
	}
	if (const AController* Controller = Cast<AController>(Actor))
	{
		return Controller;
	}
	return nullptr;
}

bool UAFLLootGrantComponent::IsEligible(const AActor* Retriever, const AController* RetrieverController) const
{
	switch (Eligibility)
	{
	case EAFLLootEligibility::Anyone:
		return true;
	case EAFLLootEligibility::EnemyOnly:
		// The owner was already short-circuited in TryGrant (the owner seam), so any retriever that reaches
		// here is a non-owner = an enemy = eligible. MIRRORS the head/limb "non-owner -> grant" path.
		return true;
	case EAFLLootEligibility::TeamOnly:
		// Q1 team-gating: eligible iff the retriever is on the owner's team. The Lyra team compare is wired in
		// Phase 3 with the first TEAM-gated cache (its consumer); no loot is TeamOnly this phase, so this path
		// is dormant. Permissive default until then (documented), never silently wrong for a live consumer.
		return true;
	default:
		return false;
	}
}

void UAFLLootGrantComponent::GrantValue(AActor* Retriever)
{
	switch (ValueModel)
	{
	case EAFLLootValueModel::Watts:
	{
		// ECONOMY -- the proven combat-loot path: retriever pawn -> PlayerState -> wallet -> EarnWattsAuthority
		// (server-auth CommitMutation funnel; GrantReason names the diag line). LIFTED from head/limb OnGrabbedBy.
		const APawn* Pawn = Cast<APawn>(Retriever);
		APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
		if (UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr)
		{
			Wallet->EarnWattsAuthority(LootValue, *GrantReason);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOT: +%d W -> %s (%s)"),
				LootValue, *GetNameSafe(Retriever), *GrantReason);
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOOT: %s looted but no wallet -- no Watts granted (%s)"),
				*GetNameSafe(Retriever), *GrantReason);
		}
		break;
	}
	case EAFLLootValueModel::CarryToExtractEnergy:
	{
		// Loot-Carry Phase B: the value enters the retriever's CARRIED-at-risk pool (UAFLLootCarryComponent;
		// banks to Watts only at the extraction zone), NOT the wallet. Mirrors the Watts case's pawn
		// resolution, routed to the pool component. The caches Configure THIS model now (Watts was instant-bank).
		APawn* Pawn = Cast<APawn>(Retriever);
		if (UAFLLootCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<UAFLLootCarryComponent>() : nullptr)
		{
			if (ScatterGibMesh)
			{
				// V7-1: a dismember PART enters the pool as an INDIVISIBLE token (owner = the victim's player-id,
				// the specific zone, the fixed value) -- it NEVER touches the chunking rail, so it can't fragment.
				// Owner + zone RIDE for the V7-2 owner-reattach (not routed in V7-1).
				FAFLCarriedPart Token;
				Token.OwnerPlayerId = UAFLLootCarryComponent::ResolvePlayerId(OwnerActor.Get());
				Token.OriginZone    = ScatterZone;
				Token.FixedValue    = LootValue;
				Token.GibMesh       = ScatterGibMesh;
				Token.GibMaterial   = ScatterGibMaterial;
				Token.GibSkinColor  = ScatterGibSkinColor;
				Carry->CollectPart(Token);
			}
			else
			{
				// A fungible CACHE -> the value rail (UNCHANGED -- the cube form + the chunking spread, byte-identical).
				Carry->Collect(LootValue, Carry->MakeLimbForm(ScatterGibMesh, ScatterGibMaterial, ScatterGibSkinColor));
			}
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOT: +%d -> %s carried pool (%s)"),
				LootValue, *GetNameSafe(Retriever), *GrantReason);
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOOT: %s looted but no UAFLLootCarryComponent -- not pooled (%s)"),
				*GetNameSafe(Retriever), *GrantReason);
		}
		break;
	}
	case EAFLLootValueModel::GameplayResource:
		// Wired with its phase (the IPickupable bridge = Phase 5). No live consumer routes this yet -- reserved.
		UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_LOOT: value model %d not yet wired (reserved for its phase)"),
			static_cast<int32>(ValueModel));
		break;
	case EAFLLootValueModel::Reattach:
		// Never reaches here -- the owner seam (OnOwnerRetrieved) handles reattach; a non-owner of a Reattach
		// loot has nothing to grant. (Defensive no-op.)
		break;
	default:
		break;
	}
}

bool UAFLLootGrantComponent::TryGrant(AActor* Retriever)
{
	const AActor* MyOwner = GetOwner();
	// Server-authoritative consequence; guard so a replicated/late substrate bind can never double-fire.
	if (!MyOwner || !MyOwner->HasAuthority())
	{
		return false;
	}

	// E2 cause-A (RACE fix, defense-in-depth): never grant on UNCONFIGURED loot. A substrate (overlap/grab) that
	// fires before Initialize->Configure has no OwnerActor/LootValue -> the owner would self-collect for +0. The
	// IsViableCollector gate already blocks the overlap via ResolveRetrievalMode; this guards every TryGrant path.
	if (!bConfigured)
	{
		return false;
	}

	// Grant the loot EXACTLY ONCE -- the grant substrate can re-broadcast (drop+regrab, multi-activation,
	// proximity re-grant), which without this guard paid the value every time (PIE-watched +20 ~15x for one
	// limb before the fix). LIFTED from the head/limb bCollected guard.
	if (bSpent)
	{
		return false;
	}

	const AController* RetrieverController = ResolveController(Retriever);

	// OWNER seam: if the retriever IS the loot's owner (controller match -- the stable identity across respawn),
	// the owner never gets a grant. Fire OnOwnerRetrieved so the consumer runs its owner action (dismember:
	// RestoreZone + Destroy). For ownerless loot (caches/energy, OwnerActor null) this never fires.
	if (OwnerActor.IsValid())
	{
		const AController* OwnerController = ResolveController(OwnerActor.Get());
		if (OwnerController != nullptr && RetrieverController == OwnerController)
		{
			OnOwnerRetrieved.Broadcast(Retriever);
			return false;
		}
	}

	// Non-owner: eligibility (Anyone / EnemyOnly = yes; TeamOnly = same team, Phase 3). A reject = no grant,
	// no owner action (e.g. an enemy hitting a TeamOnly cache).
	if (!IsEligible(Retriever, RetrieverController))
	{
		return false;
	}

	GrantValue(Retriever);
	bSpent = true;
	OnLootGranted.Broadcast(Retriever, LootValue);
	return true;
}

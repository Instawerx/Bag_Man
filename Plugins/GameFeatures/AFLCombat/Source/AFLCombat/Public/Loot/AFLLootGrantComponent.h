// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "Loot/AFLLootTypes.h"

#include "AFLLootGrantComponent.generated.h"

class AController;

/** Fired (server-auth) when the OWNER of an owner-branch loot retrieves their own loot -- the seam the
 *  CONSUMER binds to its owner action (dismember binds it -> RestoreZone + Destroy). The component itself
 *  never knows about RestoreZone; this delegate is the dependency-inversion that keeps it generic + in
 *  AFLCombat (no circular dep on AFLDismember). Never a grant -- the owner is granted nothing. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAFLOnLootOwnerRetrieved, AActor*, Retriever);

/** Fired (server-auth) after a SUCCESSFUL value grant (post-grant logging / consumer hooks). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAFLOnLootGranted, AActor*, Retriever, int32, Value);

/**
 * UAFLLootGrantComponent -- the GENERALIZED loot grant (Loot Phase 1). The {eligibility branch +
 * grant-once guard + value grant} lifted VERBATIM from the proven head/limb OnGrabbedBy (committed
 * 9ac3e0ae). A loot object = a retrieval substrate (UAFLGrabbableComponent for Carry; overlap/harvest
 * components later) + this component + a config. The substrate calls TryGrant(Retriever) on retrieval.
 *
 * DEPENDENCY INVERSION (the load-bearing design, operator-blessed): this component is GENERIC and lives
 * in AFLCombat (it has the wallet). The owner-branch action (dismember's RestoreZone) is DISMEMBER-specific
 * and would be a circular dep if called from here -- so the owner-retrieve case fires the OnOwnerRetrieved
 * delegate, and the consumer (AAFLHeadLootBox / AAFLDismemberedLimb) binds it -> RestoreZone + Destroy.
 * Caches/nodes reuse this same component with their own (or no) owner binding.
 *
 * Server-authoritative (TryGrant no-ops off-authority) + grant-once (IsSpent). DETERMINISTIC: the value
 * is configured (known), never rolled -- IRONICS_ECONOMY_SPEC.md NO-RANDOMIZED-ACQUISITION.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLLootGrantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLLootGrantComponent();

	/** Configure at spawn (server). OwnerActor = the loot's owner (for EnemyOnly/TeamOnly + the owner seam);
	 *  null for ownerless Anyone loot. Reason names the wallet diag line ("head-loot"/"limb-loot"/...). */
	void Configure(EAFLLootValueModel InValueModel, int32 InValue, EAFLLootEligibility InEligibility,
		AActor* InOwnerActor, FName InGrantReason);

	/** Server-auth retrieval entry. Runs: grant-once -> owner-seam (OnOwnerRetrieved, no grant) -> eligibility
	 *  -> value grant (+ OnLootGranted). Returns true iff a value was granted. The substrate calls this on grab/
	 *  overlap/harvest-tick. MIRRORS the proven head/limb OnGrabbedBy decision flow exactly. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loot")
	bool TryGrant(AActor* Retriever);

	/** True once a grant has fired (grant-once guard -- prevents drop+regrab / re-broadcast double-pay). */
	UFUNCTION(BlueprintPure, Category = "AFL|Loot")
	bool IsSpent() const { return bSpent; }

	/** The OWNER-retrieve seam (the dismember reattach binds here). See class doc. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Loot")
	FAFLOnLootOwnerRetrieved OnOwnerRetrieved;

	/** Post-grant hook (logging / consumer reactions). */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Loot")
	FAFLOnLootGranted OnLootGranted;

private:
	/** Resolve the stable controller for a retriever/owner actor (pawn -> controller; controller -> self). */
	static const AController* ResolveController(const AActor* Actor);

	/** Eligibility AFTER the owner check (a non-owner): Anyone/EnemyOnly = yes; TeamOnly = same team (Phase 3). */
	bool IsEligible(const AActor* Retriever, const AController* RetrieverController) const;

	/** Dispatch the configured value model to the retriever. Watts = wallet EarnWattsAuthority (LIVE);
	 *  energy/gameplay-resource = wired with their phase; Reattach never reaches here (owner seam). */
	void GrantValue(AActor* Retriever);

	EAFLLootValueModel ValueModel = EAFLLootValueModel::Watts;
	int32 LootValue = 0;
	EAFLLootEligibility Eligibility = EAFLLootEligibility::Anyone;
	TWeakObjectPtr<AActor> OwnerActor;
	FString GrantReason = TEXT("loot");
	bool bSpent = false;
};

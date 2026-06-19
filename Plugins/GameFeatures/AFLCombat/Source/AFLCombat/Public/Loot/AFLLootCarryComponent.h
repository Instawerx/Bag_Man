// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"   // FGameplayMessageListenerHandle
#include "Templates/SubclassOf.h"

#include "AFLLootCarryComponent.generated.h"

class AAFLLootCarryPickup;
class UStaticMesh;
class UMaterialInterface;   // PRESENTATION: the dismember gib's slot-1 MIC carried in the scatter form
struct FAFLHitConfirmMessage;
struct FLyraVerbMessage;

/** Broadcast whenever the carried-loot pool changes (authority commit + each client OnRep). A future carried-
 *  loot HUD binds this -> event-driven readout, NEVER tick (mirrors UAFLWalletComponent::OnWalletChanged). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAFLOnCarriedValueChanged, int32, CarriedValue);

/**
 * FAFLCarriedForm  (Loot-Carry Model, Phase C1 -- one PROVENANCE bucket in the carried pool's server-only ledger)
 *
 * How much carried value rides a given scatter FORM, so scatter can spawn form-accurately (a cache cube vs a
 * limb gib) WITHOUT making the value non-fungible. Plain USTRUCT -- NOT net-serialized (no NetSerializer):
 * scatter is authority-only, so the ledger never replicates; only the int rail (CarriedValue) does
 * (the v4 skill-grounded resolution: `unreal-engine-expert` -- the ledger is read only on the server).
 */
USTRUCT()
struct FAFLCarriedForm
{
	GENERATED_BODY()

	/** The recoverable pickup class this value scatters AS (null -> the component's default cube pickup). */
	UPROPERTY()
	TSubclassOf<AAFLLootCarryPickup> ScatterForm;

	/** Optional per-spawn mesh the scattered pickup wears (a limb gib in C2/C3; null = the pickup's own cube). */
	UPROPERTY()
	TObjectPtr<UStaticMesh> GibMesh = nullptr;

	/** PRESENTATION: the victim's slot-1 MIC for a dismember gib (so the scattered pickup is SKINNED like the
	 *  fresh gib). Part of the bucket IDENTITY -- distinct skins bucket separately. Null = the mesh's own material. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> GibMaterial = nullptr;

	/** Accumulated carried value in this form bucket. Invariant (authority): sum over buckets == CarriedValue. */
	UPROPERTY()
	int32 Value = 0;
};

/**
 * UAFLLootCarryComponent  (Loot-Carry Model, Phase A -- the carried-at-risk pool)
 *
 * Per-player carried-loot pool, server-authoritative. Fungible carried VALUE is currency-like, so it rides a
 * wallet-style replicated INTEGER rail -- the at-risk TWIN of UAFLWalletComponent's banked Volts/Watts -- NOT
 * a Lyra inventory item (per afl-cpp-lyra-developer + lyra-skin-builder-marketplace: currency = a wallet rail;
 * inventory = discrete owned things. Lyra's GetTotalItemCountByDefinition counts INSTANCES, never sums value).
 *
 * - `Collect(value)`  -> CarriedValue += value                          (the COLLECT side; pickups call it)
 * - `GetCarriedValue` -> CarriedValue                                    (read the pool; replicated)
 * - Event.Damage.Confirmed on the owner -> scatter a PORTION (afl.Loot.DropPercent) as recoverable pickups
 * - OnDeathStarted (owner's health) -> scatter the FULL remainder
 * - Event.Extraction.Complete by the owner -> bank the pool into the wallet (EarnWattsAuthority) + zero it
 *
 * Mirrors UAFLWalletComponent's rail EXACTLY (SetIsReplicatedByDefault + DOREPLIFETIME + OnRep + a single
 * authority commit funnel) -- minus persistence: the at-risk pool is EPHEMERAL by design (it scatters on
 * damage/death, banks on extract; only the WALLET persists). Lives on the PAWN (at-risk dies with the pawn),
 * where the wallet lives on the PlayerState (banked survives death).
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLLootCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLLootCarryComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Pool changed -- a future carried-loot HUD binds this (event-driven; mirrors the wallet's OnWalletChanged). */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Loot")
	FAFLOnCarriedValueChanged OnCarriedValueChanged;

	/** Server-auth: add Value to the carried pool as the default CUBE form. Back-compat -- the proven Phase A/B
	 *  caches call this; it routes through the form-aware Collect below so every collect buckets identically. */
	void Collect(int32 Value);

	/** Server-auth: add Value to the carried pool, bucketed by Form in the provenance ledger so it scatters as
	 *  that form (cube vs limb gib). The dismember migration (C3) calls this with the limb's gib form. */
	void Collect(int32 Value, const FAFLCarriedForm& Form);

	/** Build the dismember scatter FORM (C2): the SAME recoverable carry-pickup (ScatterPickupClass) as the cube,
	 *  but WEARING a limb GIB MESH (set per-spawn via the pickup's replicated SetVisualMesh) -- NOT a re-spawned
	 *  reattachable AAFLDismemberedLimb (that carries the owner-reattach branch + a double-grant risk; this is the
	 *  lighter loot that merely LOOKS like the limb). C3 passes the live limb's own gib mesh (LimbGibMesh /
	 *  HeadGibMesh); the form then rides the proven Collect(value, form) + C1 ledger/scatter path unchanged. */
	FAFLCarriedForm MakeLimbForm(UStaticMesh* GibMesh, UMaterialInterface* GibMaterial = nullptr) const;

	/** The carried-loot pool (replicated; the owner's HUD reads this). */
	UFUNCTION(BlueprintPure, Category = "AFL|Loot")
	int32 GetCarriedValue() const { return CarriedValue; }

protected:
	/** The recoverable pickup spawned on scatter (defaults to AAFLLootCarryPickup; overridable on a BP child). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	TSubclassOf<AAFLLootCarryPickup> ScatterPickupClass;

	/** The carried-loot pool -- fungible currency-at-risk, a replicated INTEGER rail (mirrors the wallet's
	 *  Volts/Watts; off-GAS; pool changes are collect/scatter/extract-rare). Banks into the wallet on extract. */
	UPROPERTY(ReplicatedUsing = OnRep_CarriedValue)
	int32 CarriedValue = 0;

	UFUNCTION()
	void OnRep_CarriedValue();

	/** Death -> scatter the remainder. */
	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	void HandleDamageConfirmed(FGameplayTag Channel, const FAFLHitConfirmMessage& Message);
	void HandleExtractionComplete(FGameplayTag Channel, const FLyraVerbMessage& Message);

	/** Remove Amount from the pool + scatter it as K recoverable pickups near the owner (anyone can recover). */
	void ScatterValue(int32 Amount);

private:
	/** The single authority commit point for the pool: clamp >=0, replicate (DOREPLIFETIME), broadcast. Mirrors
	 *  UAFLWalletComponent::CommitMutation minus persistence (the at-risk pool is ephemeral). Listen-host applies
	 *  locally here (OnRep does not fire on authority) -> broadcast for the host HUD too. */
	void CommitCarriedDelta(int32 Delta);

	/** Add Value to the ledger bucket matching Form's identity (class + gib mesh), else open a new bucket.
	 *  Authority bookkeeping that mirrors the rail delta (keeps sum(Ledger) == CarriedValue). */
	void BucketValue(const FAFLCarriedForm& Form, int32 Value);

	/** Spawn recoverable pickups of ONE form summing to Value (the proven per-chunk quantization + the gib-mesh
	 *  override). Shared by every drained bucket in ScatterValue + the desync safety net. Authority. */
	void SpawnFormPickups(TSubclassOf<AAFLLootCarryPickup> Form, UStaticMesh* GibMesh, UMaterialInterface* GibMaterial, int32 Value);

	/** SERVER-ONLY provenance ledger (NOT replicated -- scatter is authority-only): which carried value rides
	 *  which scatter form, so scatter spawns form-accurately. sum(Value) == CarriedValue on the authority; the
	 *  rail (CarriedValue) stays the ONLY replicated property (the v4 skill-grounded resolution). Bounded (~4). */
	UPROPERTY()
	TArray<FAFLCarriedForm> Ledger;

	FGameplayMessageListenerHandle DamageListenerHandle;
	FGameplayMessageListenerHandle ExtractListenerHandle;
};

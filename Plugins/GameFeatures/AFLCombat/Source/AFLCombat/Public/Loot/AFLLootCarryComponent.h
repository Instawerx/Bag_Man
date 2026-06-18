// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"   // FGameplayMessageListenerHandle
#include "Templates/SubclassOf.h"

#include "AFLLootCarryComponent.generated.h"

class AAFLLootCarryPickup;
struct FAFLHitConfirmMessage;
struct FLyraVerbMessage;

/** Broadcast whenever the carried-loot pool changes (authority commit + each client OnRep). A future carried-
 *  loot HUD binds this -> event-driven readout, NEVER tick (mirrors UAFLWalletComponent::OnWalletChanged). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAFLOnCarriedValueChanged, int32, CarriedValue);

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

	/** Server-auth: add Value to the carried-loot pool (the at-risk twin of the banked wallet). Pickups call it. */
	void Collect(int32 Value);

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

	FGameplayMessageListenerHandle DamageListenerHandle;
	FGameplayMessageListenerHandle ExtractListenerHandle;
};

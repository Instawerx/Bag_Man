// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "AFLStoreRotationAsset.generated.h"

/**
 * The store's rotating offer: which catalog rows are for sale in the current window, and which of
 * them are featured.
 *
 * DETERMINISTIC FROM (POOL, WINDOW), NOT RANDOM AND NOT REPLICATED. The offer is computed the same
 * way on the server and on every client, so it needs no replication and no authority round-trip --
 * and, more importantly, a player who reopens the store mid-window sees the SAME offer. A rolled
 * offer would either need to be stored and replicated, or it would reshuffle under the player's
 * hands, which reads as items disappearing from the shop.
 *
 * ⚠ NO PRICES HERE. A row's price lives on the catalog entry, and bundles are priced in the
 * mint-ledger row that ServerRequestBundlePurchase reads. A price in a rotation asset would be a
 * third place for one to live, and this project has already spent three sessions on a price that
 * survived in a doc after being superseded.
 *
 * ⚠ BUNDLES ARE NOT BUILT HERE. ServerRequestBundlePurchase + the mint ledger already ship them,
 * HMAC-signed and atomic with refund (its own comment records that a client-side grant was
 * considered and rejected). A rotation may OFFER a bundle id; it must never grant one.
 */
UCLASS(BlueprintType)
class AFLLIVEOPS_API UAFLStoreRotationAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Rotation anchor. Window 0 begins here; every window is PeriodDays long. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Store|Rotation")
	FDateTime EpochUtc;

	/** Window length. 7 = weekly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Store|Rotation", meta = (ClampMin = "1"))
	int32 PeriodDays = 7;

	/** How many rows are on offer in a window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Store|Rotation", meta = (ClampMin = "1"))
	int32 SlotCount = 6;

	/** How many of the offered rows are FEATURED. Must be <= SlotCount. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Store|Rotation", meta = (ClampMin = "0"))
	int32 FeaturedCount = 2;

	/** Candidate catalog ids. Rotation selects from these; it never invents an id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Store|Rotation")
	TArray<FName> Pool;

	/** Which window @NowUtc falls in. Negative before the epoch, which ValidateRotation flags. */
	UFUNCTION(BlueprintPure, Category = "AFL|Store|Rotation")
	int32 WindowIndexAt(const FDateTime& NowUtc) const;

	/**
	 * The offer for @WindowIndex, in stable order.
	 *
	 * Featured are the FIRST FeaturedCount entries -- one selection, one order, so featured is a
	 * prefix rather than a second draw that could disagree with the first about what is on sale.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Store|Rotation")
	TArray<FName> GetOfferForWindow(int32 WindowIndex) const;

	/** The featured prefix of the same offer. Always a subset, by construction. */
	UFUNCTION(BlueprintPure, Category = "AFL|Store|Rotation")
	TArray<FName> GetFeaturedForWindow(int32 WindowIndex) const;

	/**
	 * What must be true of a shippable rotation, as failures.
	 *
	 * Returns them rather than logging, for the same reason ValidateSeason does: a validator that
	 * only logs cannot be asserted against.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Store|Rotation")
	TArray<FString> ValidateRotation() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("AFLStoreRotation"), GetFName());
	}
};

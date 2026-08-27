// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLStoreRotationAsset.h"

#include "AFLLiveOps.h"

int32 UAFLStoreRotationAsset::WindowIndexAt(const FDateTime& NowUtc) const
{
	if (PeriodDays <= 0)
	{
		return 0;
	}

	// FLOOR, NOT TRUNCATE. Integer division truncates toward zero, so a time BEFORE the epoch would
	// map -0.5 windows to 0 -- the same window as the first real one. Two different instants sharing
	// a window index is how a "rotation" silently stops rotating at the boundary.
	const double Days = (NowUtc - EpochUtc).GetTotalDays();
	return static_cast<int32>(FMath::FloorToDouble(Days / static_cast<double>(PeriodDays)));
}

TArray<FName> UAFLStoreRotationAsset::GetOfferForWindow(const int32 WindowIndex) const
{
	TArray<FName> Offer;
	if (Pool.Num() == 0 || SlotCount <= 0)
	{
		return Offer;
	}

	// SEEDED BY THE WINDOW AND BY THE POOL ITSELF.
	//
	// The window alone would keep an offer identical after a designer edits the pool, so a removed
	// item could stay on sale. Folding a pool fingerprint into the seed means changing the pool
	// changes the draw -- which is the behaviour a designer expects when they edit it.
	uint32 PoolHash = 0u;
	for (const FName& Id : Pool)
	{
		PoolHash = HashCombine(PoolHash, GetTypeHash(Id));
	}
	// Window index is signed and may be negative before the epoch; hash it rather than seeding with
	// it directly so a negative index is still a stable, distinct seed.
	FRandomStream Stream(static_cast<int32>(HashCombine(PoolHash, GetTypeHash(WindowIndex))));

	// PARTIAL FISHER-YATES over indices. Shuffling indices rather than sampling repeatedly is what
	// makes duplicates structurally impossible -- a "draw N random entries" loop can pick the same
	// row twice, and the store would show one item in two slots.
	TArray<int32> Idx;
	Idx.Reserve(Pool.Num());
	for (int32 i = 0; i < Pool.Num(); ++i) { Idx.Add(i); }

	const int32 Take = FMath::Min(SlotCount, Idx.Num());
	for (int32 i = 0; i < Take; ++i)
	{
		const int32 j = Stream.RandRange(i, Idx.Num() - 1);
		Idx.Swap(i, j);
		Offer.Add(Pool[Idx[i]]);
	}
	return Offer;
}

TArray<FName> UAFLStoreRotationAsset::GetFeaturedForWindow(const int32 WindowIndex) const
{
	// A PREFIX OF THE SAME OFFER, not a second draw. Two independent draws could disagree about what
	// is on sale -- featuring a row the store is not offering this window.
	const TArray<FName> Offer = GetOfferForWindow(WindowIndex);
	const int32 Take = FMath::Clamp(FeaturedCount, 0, Offer.Num());

	TArray<FName> Featured;
	Featured.Reserve(Take);
	for (int32 i = 0; i < Take; ++i)
	{
		Featured.Add(Offer[i]);
	}
	return Featured;
}

TArray<FString> UAFLStoreRotationAsset::ValidateRotation() const
{
	TArray<FString> Fail;

	if (Pool.Num() == 0)
	{
		Fail.Add(TEXT("Pool is empty -- the store would offer nothing."));
	}
	if (PeriodDays <= 0)
	{
		Fail.Add(FString::Printf(TEXT("PeriodDays is %d -- must be >= 1, or every instant is window 0 "
		                              "and the rotation never rotates."), PeriodDays));
	}
	if (SlotCount <= 0)
	{
		Fail.Add(FString::Printf(TEXT("SlotCount is %d -- must be >= 1."), SlotCount));
	}
	if (FeaturedCount < 0 || FeaturedCount > SlotCount)
	{
		Fail.Add(FString::Printf(
			TEXT("FeaturedCount is %d against SlotCount %d -- featured must be a subset of the offer."),
			FeaturedCount, SlotCount));
	}
	if (Pool.Num() > 0 && SlotCount > Pool.Num())
	{
		// NOT FATAL, and said plainly: the offer simply becomes the whole pool. Reported because a
		// designer expecting 6 slots from a 4-row pool otherwise sees a short store with no reason.
		Fail.Add(FString::Printf(
			TEXT("SlotCount %d exceeds pool size %d -- every window will offer the entire pool, so "
			     "the store will not appear to rotate."), SlotCount, Pool.Num()));
	}

	TSet<FName> Seen;
	for (const FName& Id : Pool)
	{
		if (Id.IsNone())
		{
			Fail.Add(TEXT("Pool contains a None id."));
			continue;
		}
		bool bDup = false;
		Seen.Add(Id, &bDup);
		if (bDup)
		{
			// A duplicated pool row raises its own draw odds and can appear twice in one window --
			// the shuffle guarantees distinct INDICES, not distinct ids.
			Fail.Add(FString::Printf(TEXT("Pool contains '%s' more than once."), *Id.ToString()));
		}
	}

	if (EpochUtc.GetTicks() == 0)
	{
		Fail.Add(TEXT("EpochUtc is unset -- windows would be measured from year 1."));
	}

	return Fail;
}

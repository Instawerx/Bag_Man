// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_WeaponWheel.h"

#include "AFLCombat.h"                     // LogAFLCombat
#include "AFLCosmeticCatalogSubsystem.h"   // per-weapon tint -- the SAME resolver the store tile uses
#include "AFLCosmeticCoreTypes.h"          // FAFLCatalogEntry
#include "UI/AFLUITheme.h"                 // chrome fallback (SSOT -- no copied literals)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_WeaponWheel)

void UAFLW_WeaponWheel::ConfigureRing(int32 SlotCount)
{
	const int32 N = FMath::Clamp(SlotCount, 0, FMath::Max(2, WheelCapacity));
	Segments.Reset(N);

	// Even division over N, slot 0 centred at 12 o'clock. Generalised deliberately: a hardcoded segment
	// count is what makes a wheel painful to extend, and the roster is actively growing.
	const float SegmentDeg = (N > 0) ? (360.f / static_cast<float>(N)) : 360.f;
	for (int32 i = 0; i < N; ++i)
	{
		FAFLWheelSegment S;
		S.SlotIndex = i;
		S.CentreAngleDeg = SegmentDeg * static_cast<float>(i);
		S.Tint = AFLUITheme::CyanActive;   // in-theme until the WBP supplies the real per-weapon tint
		Segments.Add(MoveTemp(S));
	}

	HoveredIndex = INDEX_NONE;
}

void UAFLW_WeaponWheel::SetSegmentContent(int32 Index, bool bOccupied, FText DisplayName, FSlateBrush Icon, FLinearColor Tint)
{
	if (!Segments.IsValidIndex(Index))
	{
		return;
	}
	FAFLWheelSegment& S = Segments[Index];
	S.bOccupied  = bOccupied;
	S.DisplayName = MoveTemp(DisplayName);
	S.Icon        = MoveTemp(Icon);
	S.Tint        = Tint;
}

float UAFLW_WeaponWheel::GetSegmentAngle(int32 Index) const
{
	return Segments.IsValidIndex(Index) ? Segments[Index].CentreAngleDeg : 0.f;
}

int32 UAFLW_WeaponWheel::UpdatePointer(FVector2D Pointer)
{
	const int32 N = Segments.Num();
	if (N <= 0)
	{
		return INDEX_NONE;
	}

	// DEADZONE: inside it the pointer is "undecided" and the hover HOLDS (we do NOT clear it). Clearing here
	// would mean flicking through centre on the way to a target wipes the selection, and releasing near centre
	// would swap to whatever was last brushed -- both feel broken in a fight.
	if (Pointer.SizeSquared() < (SelectDeadzone * SelectDeadzone))
	{
		return HoveredIndex;
	}

	// 0 deg = straight up, increasing CLOCKWISE, matching how the ring is laid out in ConfigureRing.
	// Atan2(X, Y) rather than the usual (Y, X): that swap is what puts 0 at +Y (up) and winds clockwise.
	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Pointer.X, Pointer.Y));
	if (AngleDeg < 0.f)
	{
		AngleDeg += 360.f;
	}

	// Half-segment offset so slot 0's SLICE straddles 12 o'clock symmetrically -- without it the boundary
	// sits dead centre at the top, which is the most common place a thumb rests.
	const float SegmentDeg = 360.f / static_cast<float>(N);
	const int32 NewIndex = FMath::Clamp(
		static_cast<int32>(FMath::Fmod(AngleDeg + (SegmentDeg * 0.5f), 360.f) / SegmentDeg), 0, N - 1);

	if (NewIndex != HoveredIndex)
	{
		const int32 OldIndex = HoveredIndex;
		HoveredIndex = NewIndex;
		OnHoverChanged(NewIndex, OldIndex);   // fires only on CHANGE, so the pulse doesn't retrigger per frame
	}
	return HoveredIndex;
}

void UAFLW_WeaponWheel::ResetHover()
{
	HoveredIndex = INDEX_NONE;
}

int32 UAFLW_WeaponWheel::GetSlotToCommit(int32 CurrentActiveSlot) const
{
	// Every "do nothing" case collapses here so the WBP calls the server RPC only when it would actually
	// change something: no hover (never left the deadzone), an empty slot, or the weapon already equipped.
	if (!Segments.IsValidIndex(HoveredIndex))
	{
		return INDEX_NONE;
	}
	const FAFLWheelSegment& S = Segments[HoveredIndex];
	if (!S.bOccupied || S.SlotIndex == CurrentActiveSlot)
	{
		return INDEX_NONE;
	}
	return S.SlotIndex;
}

FLinearColor UAFLW_WeaponWheel::ResolveTintForCosmeticId(FName CosmeticId) const
{
	// Same resolve the store tile uses (GetEntryPrimaryColor over the catalog) -- the wheel must never hold a
	// baked colour. An unmapped weapon falls back to the shared theme accent so it still reads in-family
	// instead of blowing out white.
	if (CosmeticId.IsNone())
	{
		return AFLUITheme::CyanActive;
	}

	if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this))
	{
		if (const FAFLCatalogEntry* Entry = Catalog->FindEntry(CosmeticId))
		{
			return UAFLCosmeticCatalogSubsystem::GetEntryPrimaryColor(this, *Entry);
		}
	}

	UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_WHEEL: no catalog entry for '%s' -> theme accent fallback."),
		*CosmeticId.ToString());
	return AFLUITheme::CyanActive;
}

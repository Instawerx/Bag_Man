// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_WeaponWheel.h"

#include "AFLCombat.h"                     // LogAFLCombat
#include "AFLCosmeticCatalogSubsystem.h"   // per-weapon tint -- the SAME resolver the store tile uses
#include "AFLCosmeticCoreTypes.h"          // FAFLCatalogEntry
#include "UI/AFLUITheme.h"                 // chrome fallback (SSOT -- no copied literals)

#include "Components/CanvasPanel.h"        // BindWidget target

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_WeaponWheel)

void UAFLW_WeaponWheel::BeginRing()
{
	Segments.Reset();

	// Focus is a ring POSITION, and every position is about to be invalidated. Clearing it means the first
	// ReflectActiveSlot after a rebuild always counts as a change and re-punches the equipped weapon --
	// otherwise a rebuild that happens to land the same index would leave the ring visually unhighlighted.
	FocusedIndex = INDEX_NONE;
}

int32 UAFLW_WeaponWheel::AddFilledSegment(int32 QuickBarSlotIndex)
{
	if (Segments.Num() >= FMath::Max(2, WheelCapacity))
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_WHEEL: ring at capacity (%d) -- quickbar slot %d not shown."),
			WheelCapacity, QuickBarSlotIndex);
		return INDEX_NONE;
	}

	FAFLWheelSegment S;
	S.SlotIndex = QuickBarSlotIndex;   // the REAL slot, not the ring position
	S.bOccupied = true;                // only filled slots ever reach here
	S.Tint      = AFLUITheme::CyanActive;
	return Segments.Add(MoveTemp(S));
}

void UAFLW_WeaponWheel::FinalizeRing()
{
	const int32 N = Segments.Num();
	if (N <= 0)
	{
		return;
	}

	// Even division over the FILLED count -- this is what makes 3 weapons render as thirds rather than as
	// three slices of a 15-slot ring with twelve gaps.
	const float SegmentDeg = 360.f / static_cast<float>(N);
	for (int32 i = 0; i < N; ++i)
	{
		Segments[i].CentreAngleDeg = SegmentDeg * static_cast<float>(i);
	}
}

void UAFLW_WeaponWheel::SetSegmentContent(int32 Index, bool bOccupied, FText DisplayName, FSlateBrush Icon, FLinearColor Tint)
{
	if (!Segments.IsValidIndex(Index))
	{
		return;
	}
	FAFLWheelSegment& S = Segments[Index];
	S.bOccupied   = bOccupied;
	S.DisplayName = MoveTemp(DisplayName);
	S.Icon        = MoveTemp(Icon);
	S.Tint        = Tint;
}

int32 UAFLW_WeaponWheel::GetSegmentSlotIndex(int32 Index) const
{
	return Segments.IsValidIndex(Index) ? Segments[Index].SlotIndex : INDEX_NONE;
}

float UAFLW_WeaponWheel::GetSegmentAngle(int32 Index) const
{
	return Segments.IsValidIndex(Index) ? Segments[Index].CentreAngleDeg : 0.f;
}

FVector2D UAFLW_WeaponWheel::GetSegmentPosition(int32 Index, float Radius) const
{
	const float Rad = FMath::DegreesToRadians(GetSegmentAngle(Index));

	// Sin/-Cos (not Cos/Sin): 0 deg must land at 12 o'clock and wind CLOCKWISE, and screen Y grows
	// DOWNWARD -- so "up" is NEGATIVE Y.
	return FVector2D(FMath::Sin(Rad) * Radius, -FMath::Cos(Rad) * Radius);
}

int32 UAFLW_WeaponWheel::ReflectActiveSlot(int32 ActiveQuickBarSlot)
{
	// Search by the segment's REAL slot, never by position -- the ring is compacted, so ring position N and
	// quickbar slot N are unrelated the moment the player has a gap in their loadout.
	int32 NewFocus = INDEX_NONE;
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		if (Segments[i].SlotIndex == ActiveQuickBarSlot)
		{
			NewFocus = i;
			break;
		}
	}

	// Fire ONLY on change. This is called every tick, so an unconditional broadcast would restart the punch
	// animation every frame and the highlight would sit frozen at its first pose.
	if (NewFocus != FocusedIndex)
	{
		const int32 OldIndex = FocusedIndex;
		FocusedIndex = NewFocus;
		OnFocusChanged(NewFocus, OldIndex);
	}
	return FocusedIndex;
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

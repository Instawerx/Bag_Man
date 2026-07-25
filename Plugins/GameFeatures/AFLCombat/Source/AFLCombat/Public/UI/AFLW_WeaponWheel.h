// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"

#include "AFLW_WeaponWheel.generated.h"

/**
 * One rendered wheel segment. C++ owns the LAYOUT (angle) and the resolved TINT; the WBP fills in the
 * per-slot content it reads off the quickbar.
 */
USTRUCT(BlueprintType)
struct FAFLWheelSegment
{
	GENERATED_BODY()

	/** Quickbar slot this segment maps to. Segment order == slot order deliberately: the wheel and the
	 *  legacy number keys must always agree about which weapon is slot N, or muscle memory breaks. */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Wheel")
	int32 SlotIndex = INDEX_NONE;

	/** Empty slots still occupy a segment -- the ring must NOT reflow as you pick things up mid-fight. */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Wheel")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadWrite, Category = "AFL|Wheel")
	FText DisplayName;

	/** From InventoryFragment_QuickBarIcon. A white-silhouette MASK -- the wheel TINTS it. (Verified by
	 *  reading W_QuickBarSlot's graphs: the stock slot does NOT tint, so this is new behaviour here.) */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Wheel")
	FSlateBrush Icon;

	/** Per-weapon tint, resolved through the catalog -- never a baked colour. */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Wheel")
	FLinearColor Tint = FLinearColor::White;

	/** Centre angle, degrees, 0 = straight up, increasing clockwise. The WBP places by this. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Wheel")
	float CentreAngleDeg = 0.f;
};

/**
 * UAFLW_WeaponWheel -- radial weapon selector (Tier 1).
 *
 * ⚠ WHY THE SPLIT IS WHERE IT IS: ULyraQuickBarComponent and ULyraInventoryItemInstance carry NO
 * LYRAGAME_API export, so a direct C++ call from AFLCombat is an LNK2019 -- the documented Lyra-cosmetic
 * export trap (the same one that forced UAFLCharacterPartSelectorComponent onto ProcessEvent reflection).
 * Their methods ARE BlueprintCallable, so:
 *     WBP  -> touches the quickbar (GetSlots / GetActiveSlotIndex / SetActiveSlotIndex) and draws.
 *     C++  -> owns the MATH (ring layout, angle/deadzone resolve, hover state) and the TINT RESOLVE.
 * That avoids reflection entirely and keeps the fiddly, regression-prone half in C++.
 *
 * REUSE, NOT REBUILD: SetActiveSlotIndex is ALREADY a reliable server RPC, so multiplayer is inherited.
 * Nothing here touches inventory or replication.
 *
 * CAPACITY vs COUNT: the angle math is generalised over N. WheelCapacity is the DESIGN ceiling (16); the
 * ring renders however many slots the quickbar reports. Never hardcode 8 -- a fixed segment count is what
 * makes a wheel painful to extend when the roster grows.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class AFLCOMBAT_API UAFLW_WeaponWheel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Design ceiling for ONE readable ring. Past ~16 a ring needs categories/sub-rings, not more slices. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Wheel", meta = (ClampMin = "2", ClampMax = "16"))
	int32 WheelCapacity = 16;

	/** Outer-magnitude gate. Inside it the pointer is "undecided" and the hover HOLDS -- so releasing near
	 *  centre keeps the current weapon instead of swapping to whatever the stick last brushed past. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Wheel", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float SelectDeadzone = 0.3f;

	/** Hold duration before the wheel opens, so a tap stays free for something else later. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Wheel", meta = (ClampMin = "0.0"))
	float HoldTimeToOpen = 0.2f;

	/** Hover punch speed for the WBP's InterpTo feedback. Higher = snappier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Wheel", meta = (ClampMin = "1.0"))
	float SelectionPulseSpeed = 14.f;

	// ---- ring layout (C++) ----

	/** Lay out SlotCount segments evenly, slot 0 centred at 12 o'clock. Clears content; the WBP then fills
	 *  each segment via SetSegmentContent. Clamped to WheelCapacity. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	void ConfigureRing(int32 SlotCount);

	/** Fill one segment with what the WBP read off the quickbar. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	void SetSegmentContent(int32 Index, bool bOccupied, FText DisplayName, FSlateBrush Icon, FLinearColor Tint);

	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	const TArray<FAFLWheelSegment>& GetSegments() const { return Segments; }

	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	int32 GetSegmentCount() const { return Segments.Num(); }

	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	int32 GetHoveredIndex() const { return HoveredIndex; }

	/** Angle for a segment centre -- so the WBP can place without duplicating the math. */
	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	float GetSegmentAngle(int32 Index) const;

	// ---- interaction (C++) ----

	/**
	 * Feed a normalised pointer. X = right, Y = UP (the WBP normalises mouse-from-centre, or passes stick
	 * axes straight through). Returns the hovered segment, or INDEX_NONE while inside the deadzone.
	 * Fires OnHoverChanged only on an actual change, so the pulse doesn't retrigger every frame.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	int32 UpdatePointer(FVector2D Pointer);

	/** Clear hover (on open/close) without firing a spurious selection. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	void ResetHover();

	/**
	 * The slot the WBP should commit, or INDEX_NONE if it should do nothing. Returns INDEX_NONE when the
	 * pointer never left the deadzone, the segment is unoccupied, or it is already the active slot -- so
	 * the WBP calls SetActiveSlotIndex only when it would actually change something.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	int32 GetSlotToCommit(int32 CurrentActiveSlot) const;

	/** Per-weapon tint from the catalog -- the SAME subsystem the store tile resolves through. Falls back to
	 *  the theme accent so an unmapped weapon renders in-theme rather than white. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	FLinearColor ResolveTintForCosmeticId(FName CosmeticId) const;

	/** Hover changed -- drive the scale/brightness punch from here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Wheel")
	void OnHoverChanged(int32 NewIndex, int32 OldIndex);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Wheel")
	TArray<FAFLWheelSegment> Segments;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Wheel")
	int32 HoveredIndex = INDEX_NONE;
};

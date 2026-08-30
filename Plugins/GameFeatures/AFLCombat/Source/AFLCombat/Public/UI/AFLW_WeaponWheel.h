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

	/**
	 * The REAL quickbar slot this segment represents -- NOT the segment's own position in the ring.
	 *
	 * ⚠ These two diverge, and that divergence is the whole point: the ring shows only FILLED slots, evenly
	 * spaced, so a player holding weapons in quickbar slots 0, 2 and 5 sees a clean 3-segment wheel whose
	 * segments carry SlotIndex 0, 2, 5. Anything talking to the quickbar must use THIS value; anything
	 * talking to the ring geometry uses the segment's array position.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Wheel")
	int32 SlotIndex = INDEX_NONE;

	/** Always true in the filled-only model -- kept so an empty ring state is still representable. */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Wheel")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadWrite, Category = "AFL|Wheel")
	FText DisplayName;

	/** From InventoryFragment_QuickBarIcon. A white-silhouette MASK -- the wheel TINTS it. */
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
 * UAFLW_WeaponWheel -- persistent radial weapon indicator (Tier 1).
 *
 * ⚠ INTERACTION MODEL -- read this before changing anything: this wheel REPLACES the quickbar strip, so it
 * behaves like the strip. It is ALWAYS VISIBLE, it is NOT a menu you summon, and it has NO input of its own.
 * The existing mouse-wheel cycle input (IA_QuickSlot_CycleForward/Backward -> GA_QuickbarSlots ->
 * ULyraQuickBarComponent::CycleActiveSlotForward) already drives the quickbar and already skips empty slots.
 * This widget only REFLECTS ActiveSlotIndex. Input drives state; the wheel draws it. There is deliberately
 * no hover, no open/close, and no commit path -- adding one would create a second source of truth for which
 * weapon is equipped.
 *
 * ⚠ WHY THE C++/WBP SPLIT IS WHERE IT IS: ULyraQuickBarComponent and ULyraInventoryItemInstance carry NO
 * LYRAGAME_API export, so a direct C++ call from AFLCombat is an LNK2019 -- the documented Lyra-cosmetic
 * export trap. Their methods ARE BlueprintCallable, so:
 *     WBP  -> touches the quickbar (GetSlots / GetActiveSlotIndex) and draws.
 *     C++  -> owns the ring maths, the filled-slot compaction, and the focus bookkeeping.
 *
 * C++ also owns the dense counter and the last-index maths for a second, blunter reason: the editor bridge
 * cannot spawn Blueprint's promotable wildcard math nodes (Add/Subtract fail outright), so a BP-side counter
 * or a "Count - 1" is not authorable. Keeping that arithmetic here is what makes the populate loop scriptable
 * at all.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class AFLCOMBAT_API UAFLW_WeaponWheel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Design ceiling for ONE readable ring. Past ~16 a ring needs categories/sub-rings, not more slices. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Wheel", meta = (ClampMin = "2", ClampMax = "16"))
	int32 WheelCapacity = 16;

	/** Focus punch speed for the WBP's InterpTo feedback. Higher = snappier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Wheel", meta = (ClampMin = "1.0"))
	float SelectionPulseSpeed = 14.f;

	// ---- ring construction (two-pass, because spacing depends on the FINAL filled count) ----

	/**
	 * Start a rebuild: drops every segment and clears focus. Call before walking the quickbar.
	 * Pass 1 is BeginRing -> AddFilledSegment per filled slot -> FinalizeRing.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	void BeginRing();

	/**
	 * Append a segment for a FILLED quickbar slot and return its dense ring position.
	 *
	 * The WBP calls this only for slots whose item is valid, which is what collapses the gaps: empty slots
	 * never become segments, so the ring never renders holes. Angles are NOT assigned here -- even spacing
	 * cannot be known until the total filled count is, hence FinalizeRing.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	int32 AddFilledSegment(int32 QuickBarSlotIndex);

	/** Assign even 360/N spacing over however many segments were added, segment 0 at 12 o'clock. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	void FinalizeRing();

	/** Overwrite one segment's presentation (icon/name/tint) after the ring is built. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	void SetSegmentContent(int32 Index, bool bOccupied, FText DisplayName, FSlateBrush Icon, FLinearColor Tint);

	// ---- queries ----

	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	const TArray<FAFLWheelSegment>& GetSegments() const { return Segments; }

	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	int32 GetSegmentCount() const { return Segments.Num(); }

	/** Segments.Num() - 1, for driving a BP For Loop's Last Index without a subtract node. -1 when empty. */
	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	int32 GetLastSegmentIndex() const { return Segments.Num() - 1; }

	/** Dense ring position of the currently equipped weapon, or INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	int32 GetFocusedIndex() const { return FocusedIndex; }

	/** The real quickbar slot behind a ring position, or INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	int32 GetSegmentSlotIndex(int32 Index) const;

	/** Angle for a segment centre -- so the WBP can place without duplicating the math. */
	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	float GetSegmentAngle(int32 Index) const;

	/**
	 * Canvas-local position for a segment centre. Converts the ring's "0 = up, clockwise" convention into
	 * SCREEN space, where Y grows DOWNWARD -- hence the negated cosine.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Wheel")
	FVector2D GetSegmentPosition(int32 Index, float Radius) const;

	// ---- reflection (the entire interaction) ----

	/**
	 * Point the highlight at whatever the quickbar says is equipped.
	 *
	 * Takes a REAL quickbar slot and finds the segment carrying it, so the compaction in AddFilledSegment
	 * stays invisible to the caller. Fires OnFocusChanged only on an actual change, so the WBP can punch
	 * the new segment without re-triggering every tick. Returns the focused ring position.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	int32 ReflectActiveSlot(int32 ActiveQuickBarSlot);

	/** Per-weapon tint from the catalog -- the SAME subsystem the store tile resolves through. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wheel")
	FLinearColor ResolveTintForCosmeticId(FName CosmeticId) const;

	/** Focus moved to a different weapon -- drive the scale/brightness punch from here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Wheel")
	void OnFocusChanged(int32 NewIndex, int32 OldIndex);

	// ---- widget bindings ----

	/**
	 * The ring canvas the segments are added to.
	 *
	 * ⚠ WHY THIS IS BOUND IN C++ RATHER THAN JUST USED IN THE WBP: the MCP bridge cannot reference a
	 * WBP's own widget variables as graph nodes, so the segment-population loop is UNAUTHORABLE over the
	 * bridge without this. BindWidget promotes it to a real C++ property, which IS exposed. It also turns
	 * a silent null into a compile-time error if the WBP ever loses the canvas.
	 *
	 * ⚠ It must contain NOTHING at design time: segment i is addressed as WheelRoot child i, so any
	 * decorative child would shift every index by one.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Wheel", meta = (BindWidget))
	TObjectPtr<class UCanvasPanel> WheelRoot;

protected:
	/**
	 * COLD-SPAWN HEAL. The quickbar component is REPLICATED, so the client never creates it locally
	 * (GameFrameworkComponentManager refuses non-authority creation) -- it arrives from the server,
	 * and on a cold first PIE that arrival lands SECONDS after this widget constructs (server pays a
	 * long synchronous experience load, the component class itself streams in late client-side, and
	 * the replicated bunches queue behind the class load). Every populate the WBP runs before then
	 * finds no component and silently builds nothing, and the SlotsChanged message that would refresh
	 * it fires from the component's own OnRep -- which can be lost to the same window. Result: a dead
	 * wheel and no weapon cycling for the whole session.
	 *
	 * Same doctrine as the equip anim-layer fix: key on STATE, not delivery order. While the ring is
	 * empty, probe (throttled) for the component + a filled slot, and re-run the WBP's PopulateWheel
	 * when both exist. The quickbar types carry no LYRAGAME_API export, so the probe goes through
	 * reflection -- the documented Lyra export trap.
	 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Wheel")
	TArray<FAFLWheelSegment> Segments;

	/** Dense ring position of the equipped weapon. INDEX_NONE until the first ReflectActiveSlot. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Wheel")
	int32 FocusedIndex = INDEX_NONE;

private:
	/** Cold-spawn heal throttle + diagnostics (see NativeTick). */
	float HealAccumulator = 0.f;
	int32 HealAttempts = 0;
};

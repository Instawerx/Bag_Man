// AFL kit (C2, I-12/I-14/I-25): the saved-builds strip (region D). Counter "n / cap" always
// visible; over-cap builds render SAVED-LOCKED -- visible and clickable for viewing, never hidden.
// Presentational: builds in, index-clicks out. Persistence and caps live server-side (I-12).

#pragma once

#include "CommonUserWidget.h"
#include "AFLW_BuildSlotStrip.generated.h"

class UAFLW_LoadoutTileBase;
class UPanelWidget;
class UTextBlock;

USTRUCT(BlueprintType)
struct FAFLBuildSlotDesc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Builds")
	FText Name;

	/** The equipped build (at most one). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Builds")
	bool bActive = false;

	/** SAVED-LOCKED (over the free cap, I-12/I-14): stays visible, opens read-only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Builds")
	bool bLocked = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAFLBuildSlotClicked, int32, BuildIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAFLNewBuildRequested);

UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_BuildSlotStrip : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** A build click opens the creator ON that index -- builds are addressed by INDEX everywhere
	 *  (EditingIndex, ServerSaveBuild, the lapse rule); never an FName sentinel. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Builds")
	FOnAFLBuildSlotClicked OnBuildSlotClicked;

	UPROPERTY(BlueprintAssignable, Category = "AFL|Builds")
	FOnAFLNewBuildRequested OnNewBuildRequested;

	/** Rebuild the strip. UnlockedCap is the free cap the counter reads (the shipped counter law:
	 *  UNLOCKED / CAP -- a locked build sits outside the cap, so total/cap would print "5 / 2"). */
	UFUNCTION(BlueprintCallable, Category = "AFL|Builds")
	void SetBuilds(const TArray<FAFLBuildSlotDesc>& InBuilds, int32 UnlockedCount, int32 UnlockedCap);

protected:
	/** The tile class -- the PROVEN market/loadout tile (extend-not-rebuild, AFL-3221/3222): its
	 *  SetBuildData/OnBuildTileClicked build variant is already shipped. Set on the kit WBP. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Builds")
	TSubclassOf<UAFLW_LoadoutTileBase> SlotTileClass;

	/** Total slots drawn (filled + empty). The mock ratified 5. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Builds", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaxSlots = 5;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SlotsPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Counter_Text;

private:
	UFUNCTION()
	void HandleTileClicked(int32 BuildIndex);
};

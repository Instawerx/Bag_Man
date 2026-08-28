#include "UI/AFLW_BuildSlotStrip.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "UI/AFLW_LoadoutTileBase.h"

#define LOCTEXT_NAMESPACE "AFLBuildSlotStrip"

DEFINE_LOG_CATEGORY_STATIC(LogAFLBuildSlotStrip, Log, All);

void UAFLW_BuildSlotStrip::SetBuilds(const TArray<FAFLBuildSlotDesc>& InBuilds, const int32 UnlockedCount, const int32 UnlockedCap)
{
	if (Counter_Text)
	{
		// ALWAYS VISIBLE (I-12), and the shipped semantic: unlocked over the free cap.
		Counter_Text->SetText(FText::Format(LOCTEXT("SlotCounter", "BUILDS {0} / {1}"),
			FText::AsNumber(UnlockedCount), FText::AsNumber(UnlockedCap)));
	}

	if (!SlotsPanel)
	{
		UE_LOG(LogAFLBuildSlotStrip, Warning, TEXT("[BuildSlotStrip] SlotsPanel not bound -- the strip cannot render."));
		return;
	}
	SlotsPanel->ClearChildren();

	if (!SlotTileClass)
	{
		// Says so rather than drawing nothing -- an unset class and an empty build set are
		// indistinguishable on screen (the same ambiguity the rail's unset-row-class lesson names).
		UE_LOG(LogAFLBuildSlotStrip, Warning, TEXT("[BuildSlotStrip] SlotTileClass unset -- %d build(s) cannot be drawn."), InBuilds.Num());
		return;
	}

	for (int32 Index = 0; Index < InBuilds.Num(); ++Index)
	{
		const FAFLBuildSlotDesc& Desc = InBuilds[Index];
		if (UAFLW_LoadoutTileBase* Tile = CreateWidget<UAFLW_LoadoutTileBase>(this, SlotTileClass))
		{
			// The proven build variant: index-addressed, ReadOnly carries the saved-locked shape.
			Tile->SetBuildData(Index, Desc.Name, Desc.bLocked, Desc.bActive);
			Tile->OnBuildTileClicked.RemoveDynamic(this, &UAFLW_BuildSlotStrip::HandleTileClicked);
			Tile->OnBuildTileClicked.AddDynamic(this, &UAFLW_BuildSlotStrip::HandleTileClicked);
			SlotsPanel->AddChild(Tile);
		}
	}

	// One "+ NEW" affordance while a slot is free; INDEX_NONE is the tile's own non-build marker.
	if (InBuilds.Num() < MaxSlots)
	{
		if (UAFLW_LoadoutTileBase* Tile = CreateWidget<UAFLW_LoadoutTileBase>(this, SlotTileClass))
		{
			Tile->SetBuildData(INDEX_NONE, LOCTEXT("NewBuild", "NEW BUILD"), false, false);
			Tile->OnBuildTileClicked.RemoveDynamic(this, &UAFLW_BuildSlotStrip::HandleTileClicked);
			Tile->OnBuildTileClicked.AddDynamic(this, &UAFLW_BuildSlotStrip::HandleTileClicked);
			SlotsPanel->AddChild(Tile);
		}
	}
}

void UAFLW_BuildSlotStrip::HandleTileClicked(const int32 BuildIndex)
{
	if (BuildIndex == INDEX_NONE)
	{
		OnNewBuildRequested.Broadcast();
	}
	else
	{
		OnBuildSlotClicked.Broadcast(BuildIndex);
	}
}

#undef LOCTEXT_NAMESPACE

#include "UI/AFLW_PartPicker.h"

#include "AFLCosmeticCatalogSubsystem.h"
#include "Components/ListView.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "GameFramework/PlayerState.h"
#include "Player/LyraPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogAFLPartPicker, Log, All);

void UAFLW_PartPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (PartsListView)
	{
		// The market's list idiom exactly: per-item entry class + generated-widget hookup.
		PartsListView->OnGetEntryClassForItem().BindUObject(this, &UAFLW_PartPicker::GetEntryClassForItem);
		PartsListView->OnEntryWidgetGenerated().AddUObject(this, &UAFLW_PartPicker::HandleEntryGenerated);
	}
}

void UAFLW_PartPicker::SetCatalogFilter(const EAFLCosmeticType CatalogType, const EAFLLoadoutAxis ListAxis, const FName EquippedId)
{
	if (!PartsListView)
	{
		UE_LOG(LogAFLPartPicker, Warning, TEXT("[PartPicker] PartsListView not bound -- nothing to fill."));
		return;
	}

	const UGameInstance* GI = GetGameInstance();
	UAFLCosmeticCatalogSubsystem* Catalog = GI ? GI->GetSubsystem<UAFLCosmeticCatalogSubsystem>() : nullptr;
	if (!Catalog)
	{
		UE_LOG(LogAFLPartPicker, Warning, TEXT("[PartPicker] catalog subsystem unavailable -- list left empty."));
		PartsListView->ClearListItems();
		return;
	}

	// Entitlement badging via the wallet on the OWNING player's state -- resolved exactly as the
	// market resolves it (PC -> PlayerState -> FindComponentByClass). Absent wallet = everything
	// renders unowned/priced; the server still gates every commit.
	const APlayerController* PC = GetOwningPlayer();
	const ALyraPlayerState* PS = PC ? Cast<ALyraPlayerState>(PC->PlayerState) : nullptr;
	const UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;

	TArray<const FAFLCatalogEntry*> Entries;
	Catalog->GetEntriesByType(CatalogType, Entries);

	TArray<UObject*> Items;
	Items.Reserve(Entries.Num());
	for (const FAFLCatalogEntry* Entry : Entries)
	{
		if (!Entry) { continue; }
		UAFLMarketLoadoutItem* Item = NewObject<UAFLMarketLoadoutItem>(this);
		Item->Axis = ListAxis;
		Item->CosmeticId = Entry->CosmeticId;
		Item->DisplayName = Entry->DisplayName;
		Item->bEquipped = (Entry->CosmeticId == EquippedId);
		const bool bEntitled = Wallet && Wallet->IsEntitled(PS, Entry->CosmeticId);
		Item->bPurchasable = !bEntitled;
		Items.Add(Item);
	}

	PartsListView->SetListItems(Items);
	UE_LOG(LogAFLPartPicker, Log, TEXT("[PartPicker] filter type=%d axis=%d -> %d item(s), equipped=%s"),
		static_cast<int32>(CatalogType), static_cast<int32>(ListAxis), Items.Num(), *EquippedId.ToString());
}

TSubclassOf<UUserWidget> UAFLW_PartPicker::GetEntryClassForItem(UObject* /*Item*/) const
{
	return TileClass;
}

void UAFLW_PartPicker::HandleEntryGenerated(UUserWidget& EntryWidget)
{
	if (UAFLW_LoadoutTileBase* Tile = Cast<UAFLW_LoadoutTileBase>(&EntryWidget))
	{
		Tile->OnTileClicked.RemoveDynamic(this, &UAFLW_PartPicker::HandleTileClicked);
		Tile->OnTileClicked.AddDynamic(this, &UAFLW_PartPicker::HandleTileClicked);
	}
}

void UAFLW_PartPicker::HandleTileClicked(const EAFLLoadoutAxis Axis, const FName CosmeticId)
{
	// PREVIEW-ONLY BY CONSTRUCTION: broadcast and stop. The shell decides what previewing means.
	OnPartSelected.Broadcast(Axis, CosmeticId);
}

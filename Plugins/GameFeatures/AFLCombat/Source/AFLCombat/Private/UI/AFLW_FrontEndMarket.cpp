// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_FrontEndMarket.h"

#include "AFLCombat.h"                 // LogAFLCombat
#include "AFLCosmeticCoreTypes.h"      // FAFLCatalogEntry
#include "CommonButtonBase.h"          // UCommonButtonBase (the store's tabs; rewired in LOADOUT)
#include "Cosmetics/AFLCharacterPartMap.h"      // UAFLCharacterPartMap (IDENTITY id -> robot class)
#include "Cosmetics/AFLCosmeticSelectionTypes.h" // FAFLCosmeticSelection::GetActiveIdentityId
#include "Components/Border.h"         // UBorder (backdrop brushes)
#include "Components/ListView.h"       // UListView (SetListItems, OnGetEntryClassForItem, OnEntryWidgetGenerated)
#include "Components/TextBlock.h"      // relabel the tab captions
#include "Components/Widget.h"         // UWidget + GetWidgetFromName result
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Input/CommonUIInputTypes.h"  // FUIInputConfig, ECommonInputMode, EMouseCaptureMode
#include "Kismet/GameplayStatics.h"    // UGameplayStatics::GetActorOfClass
#include "UI/AFLCosmeticBrowserLibrary.h" // owned feed + equip + fan-out
#include "UI/AFLLoadoutDisplayPawn.h"  // AAFLLoadoutDisplayPawn
#include "UI/AFLW_LoadoutTileBase.h"   // UAFLW_LoadoutTileBase + UAFLMarketLoadoutItem

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_FrontEndMarket)

TOptional<FUIInputConfig> UAFLW_FrontEndMarket::GetDesiredInputConfig() const
{
	// Byte-for-byte mirror of ULyraActivatableWidget::GetDesiredInputConfig (that class is LyraGame-private).
	switch (InputConfig)
	{
	case ELyraWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case ELyraWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case ELyraWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case ELyraWidgetInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}

UWidget* UAFLW_FrontEndMarket::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Browser = GetWidgetFromName(TEXT("ShopListView")))
	{
		return Browser;
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UAFLW_FrontEndMarket::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyShowroomMode();

	// STORE mode: NO-OP -> the store WBP graph drives list/tabs/buy, byte-for-byte. Only LOADOUT overrides.
	if (Mode == EAFLMarketMode::Loadout)
	{
		EnterLoadoutMode();
	}
}

void UAFLW_FrontEndMarket::EnterLoadout()
{
	Mode = EAFLMarketMode::Loadout;
	EnterLoadoutMode();
}

void UAFLW_FrontEndMarket::ApplyShowroomMode()
{
	UWorld* World = GetWorld();
	const bool bLiveScene = World && (UGameplayStatics::GetActorOfClass(World, AAFLLoadoutDisplayPawn::StaticClass()) != nullptr);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: ApplyShowroomMode bLiveScene=%s"), bLiveScene ? TEXT("true") : TEXT("false"));
	if (!bLiveScene)
	{
		return;
	}

	static const TCHAR* const HideByName[] = {
		TEXT("GlassBlur"), TEXT("BackdropGradV"), TEXT("GlassPanel"), TEXT("BackdropGlowPool"),
		TEXT("ShowroomBG"), TEXT("ShowroomLabel")
	};
	for (const TCHAR* Name : HideByName)
	{
		if (UWidget* W = GetWidgetFromName(FName(Name)))
		{
			W->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	for (const TCHAR* BorderName : { TEXT("RootBorder"), TEXT("CenterShowroom") })
	{
		if (UBorder* B = Cast<UBorder>(GetWidgetFromName(FName(BorderName))))
		{
			B->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		}
	}
}

// ===================================================================================================
//  STEP 5 -- LOADOUT mode: OUR C++ tile in the store's ListView (no opaque-BP hooks, no graph surgery)
// ===================================================================================================

void UAFLW_FrontEndMarket::EnterLoadoutMode()
{
	if (bLoadoutActive)
	{
		return;
	}
	bLoadoutActive = true;

	// Relabel the 6 tab captions to the axis-groups (Body+Edge merged into COLORS). 5a leaves them DISABLED
	// (5b wires tab-click -> axis) so the store graph's opaque tab handler can't repopulate STORE underneath us.
	static const TCHAR* const TabLabelNames[6] = {
		TEXT("TabLabel_WEAPONS"), TEXT("TabLabel_SKINS"), TEXT("TabLabel_EMOTES"),
		TEXT("TabLabel_HELMETS"), TEXT("TabLabel_BUNDLES"), TEXT("TabLabel_VISORS") };
	static const TCHAR* const TabLabelText[6] = {
		TEXT("WEAPON"), TEXT("WEAPON-SKIN"), TEXT("BEAM"), TEXT("IDENTITY"), TEXT("COLORS"), TEXT("FACEMASK") };
	for (int32 i = 0; i < 6; ++i)
	{
		if (UTextBlock* T = Cast<UTextBlock>(GetWidgetFromName(FName(TabLabelNames[i]))))
		{
			T->SetText(FText::FromString(TabLabelText[i]));
		}
	}
	// 5b: enable the 6 tabs + rewire each click to OUR axis-switch. Clear() drops the store graph's tab binding on
	// THIS (LOADOUT) instance only -- the STORE instance is a separate push, untouched. Tabs are Lyra
	// UCommonButtonBase; a non-CommonButton tab is logged so we adjust rather than silently drop it.
	// The store's tabs are UBorder, not buttons (confirmed in PIE) -> their click is OnMouseButtonDownEvent, a
	// single-cast dynamic delegate. Enable + make each hit-testable, then BindDynamic OUR handler -- which REPLACES
	// the store's binding on this LOADOUT instance (single-cast), so there's no STORE conflict.
	auto PrepTab = [this](const TCHAR* Name) -> UBorder*
	{
		UBorder* B = Cast<UBorder>(GetWidgetFromName(FName(Name)));
		if (B)
		{
			B->SetIsEnabled(true);
			B->SetVisibility(ESlateVisibility::Visible); // hit-testable so OnMouseButtonDownEvent fires
		}
		return B;
	};
	if (UBorder* B = PrepTab(TEXT("Tab_WEAPONS"))) { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnTabWeapon); }
	if (UBorder* B = PrepTab(TEXT("Tab_SKINS")))   { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnTabWeaponSkin); }
	if (UBorder* B = PrepTab(TEXT("Tab_EMOTES")))  { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnTabBeam); }
	if (UBorder* B = PrepTab(TEXT("Tab_HELMETS"))) { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnTabIdentity); }
	if (UBorder* B = PrepTab(TEXT("Tab_BUNDLES"))) { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnTabColors); }
	if (UBorder* B = PrepTab(TEXT("Tab_VISORS")))  { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnTabFacemask); }
	for (const TCHAR* Name : { TEXT("DetailBuyVolts"), TEXT("DetailBuyWatts") })
	{
		if (UWidget* W = GetWidgetFromName(FName(Name)))
		{
			W->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Point the store's ListView at OUR tile (per-item override -> the store's default EntryWidgetClass is never
	// modified, so STORE mode is untouched), and bind each generated tile's own click delegate. Then feed owned.
	LoadoutTileClass = LoadClass<UAFLW_LoadoutTileBase>(nullptr,
		TEXT("/Game/BagMan/UI/Loadout/WBP_AFL_LoadoutTile.WBP_AFL_LoadoutTile_C"));
	if (UListView* List = Cast<UListView>(GetWidgetFromName(TEXT("ShopListView"))))
	{
		List->OnGetEntryClassForItem().BindUObject(this, &UAFLW_FrontEndMarket::GetLoadoutEntryClass);
		List->OnEntryWidgetGenerated().AddUObject(this, &UAFLW_FrontEndMarket::OnLoadoutEntryGenerated);
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MARKET: EnterLoadoutMode - 'ShopListView' not found / not a UListView."));
	}

	CurrentAxis = EAFLLoadoutAxis::BodyColor;
	PopulateForAxis(CurrentAxis);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: EnterLoadoutMode (5a) -> axis=Colors/BodyColor, tile=%s."),
		LoadoutTileClass ? *LoadoutTileClass->GetName() : TEXT("NULL"));
}

void UAFLW_FrontEndMarket::PopulateForAxis(EAFLLoadoutAxis Axis)
{
	UListView* List = Cast<UListView>(GetWidgetFromName(TEXT("ShopListView")));
	APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	if (!List || !PS)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MARKET: PopulateForAxis - missing ShopListView(%d) or PlayerState(%d)."),
			List != nullptr, PS != nullptr);
		return;
	}

	UAFLCosmeticLoadoutComponent* Loadout = GetLocalLoadout();
	const FName EquippedId = Loadout ? UAFLCosmeticBrowserLibrary::GetEquippedIdForAxis(Loadout, Axis) : NAME_None;

	TArray<FAFLCatalogEntry> Owned;
	UAFLCosmeticBrowserLibrary::GetOwnedEntriesForAxis(this, PS, Axis, Owned);

	TArray<UObject*> Items;
	Items.Reserve(Owned.Num());
	for (const FAFLCatalogEntry& Entry : Owned)
	{
		UAFLMarketLoadoutItem* Item = NewObject<UAFLMarketLoadoutItem>(this);
		Item->Axis = Axis;
		Item->CosmeticId = Entry.CosmeticId;

		// Prefer the marketing DisplayName; fall back to the CosmeticId's last token ("AFL.Finish.Crimson" -> "Crimson").
		FText Label = Entry.DisplayName;
		if (Label.IsEmpty())
		{
			const FString IdStr = Entry.CosmeticId.ToString();
			FString Left, Right;
			Label = FText::FromString(IdStr.Split(TEXT("."), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd) ? Right : IdStr);
		}
		Item->DisplayName = Label;
		Item->bEquipped = (!EquippedId.IsNone() && Entry.CosmeticId == EquippedId);
		Item->bIsSwatch = false;               // 5a: thumbnail-only (261/261 SKUs carry one); swatch fallback -> 5c
		Item->Thumbnail = Entry.ShopThumbnail;
		Items.Add(Item);
	}
	List->SetListItems(Items);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: PopulateForAxis axis=%d -> %d owned item(s) (equipped=%s)."),
		(int32)Axis, Items.Num(), *EquippedId.ToString());
}

void UAFLW_FrontEndMarket::SelectAxis(EAFLLoadoutAxis Axis)
{
	CurrentAxis = Axis;
	PopulateForAxis(Axis);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: SelectAxis -> %d"), (int32)Axis);
}

// One tiny handler per tab -- FOnPointerEvent is a single-cast dynamic delegate (no capture), so we can't share one.
#define AFL_TAB_HANDLER(FnName, AxisVal) \
	FEventReply UAFLW_FrontEndMarket::FnName(FGeometry, const FPointerEvent&) \
	{ SelectAxis(EAFLLoadoutAxis::AxisVal); return FEventReply(true); }
AFL_TAB_HANDLER(OnTabWeapon,     Weapon)
AFL_TAB_HANDLER(OnTabWeaponSkin, WeaponSkin)
AFL_TAB_HANDLER(OnTabBeam,       Beam)
AFL_TAB_HANDLER(OnTabIdentity,   Identity)
AFL_TAB_HANDLER(OnTabColors,     BodyColor)
AFL_TAB_HANDLER(OnTabFacemask,   Facemask)
#undef AFL_TAB_HANDLER

TSubclassOf<UUserWidget> UAFLW_FrontEndMarket::GetLoadoutEntryClass(UObject* /*Item*/) const
{
	// Every LOADOUT item renders as OUR tile; STORE mode never binds this delegate so its default tile is used.
	return LoadoutTileClass;
}

void UAFLW_FrontEndMarket::OnLoadoutEntryGenerated(UUserWidget& EntryWidget)
{
	if (UAFLW_LoadoutTileBase* Tile = Cast<UAFLW_LoadoutTileBase>(&EntryWidget))
	{
		// The tile owns its own click (its SelectButton -> OnTileClicked). AddUnique so pooled/regenerated tiles
		// don't stack bindings.
		Tile->OnTileClicked.AddUniqueDynamic(this, &UAFLW_FrontEndMarket::HandleLoadoutTileClicked);
	}
}

void UAFLW_FrontEndMarket::HandleLoadoutTileClicked(EAFLLoadoutAxis Axis, FName CosmeticId)
{
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: loadout tile clicked -> equip %s (axis=%d)."), *CosmeticId.ToString(), (int32)Axis);
	EquipSelected(CosmeticId, Axis);
}

void UAFLW_FrontEndMarket::EquipSelected(FName CosmeticId, EAFLLoadoutAxis Axis)
{
	UAFLCosmeticLoadoutComponent* Loadout = GetLocalLoadout();
	AAFLLoadoutDisplayPawn* Disp = GetDisplayPawn();
	APlayerController* PC = GetOwningPlayer();
	if (!Loadout || !Disp || !PC)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MARKET: EquipSelected - missing loadout(%d)/displayPawn(%d)/PC(%d)."),
			Loadout != nullptr, Disp != nullptr, PC != nullptr);
		return;
	}

	// DATA: dispatch the authority-only selection RPC from C++ (front-end is authority -> synchronous).
	UAFLCosmeticBrowserLibrary::EquipForAxis(Loadout, Axis, CosmeticId);

	// IDENTITY swaps the robot BODY (the widget's job -- not the controller comp). Resolve id -> robot class via
	// the part map (mirrors the in-match locker), IRONICS fallback. Do it BEFORE the param fan-out so the skin
	// controller re-paints the NEW body.
	if (Axis == EAFLLoadoutAxis::Identity)
	{
		static UAFLCharacterPartMap* PartMap = LoadObject<UAFLCharacterPartMap>(nullptr,
			TEXT("/Game/BagMan/Characters/Cosmetics/SkinColors/DA_AFL_CharacterPartMap.DA_AFL_CharacterPartMap"));
		const FName IdentityId = Loadout->GetSelection().GetActiveIdentityId();
		UClass* RobotCls = nullptr;
		if (PartMap && !IdentityId.IsNone())
		{
			const TSoftClassPtr<AActor> Soft = PartMap->ResolveCharacterPart(IdentityId);
			if (!Soft.IsNull())
			{
				RobotCls = Soft.LoadSynchronous();
			}
		}
		if (!RobotCls)
		{
			RobotCls = LoadClass<AActor>(nullptr, TEXT("/Game/BagMan/Characters/Cosmetics/B_AFL_Robot_IRONICS.B_AFL_Robot_IRONICS_C"));
		}
		if (RobotCls)
		{
			Disp->SetRobotBody(RobotCls);
		}
	}

	// VISUAL: fan the 5 param axes (Colors / Weapon / Weapon-Skin / Beam / Facemask) onto the DISPLAY pawn.
	UAFLCosmeticBrowserLibrary::ApplySelectionToPawn(PC, Disp);

	// Refresh the list so the EQUIPPED marker moves to the newly-equipped tile.
	PopulateForAxis(CurrentAxis);

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: equipped %s (axis=%d) -> applied to display pawn."), *CosmeticId.ToString(), (int32)Axis);
}

UAFLCosmeticLoadoutComponent* UAFLW_FrontEndMarket::GetLocalLoadout() const
{
	APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	return PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
}

AAFLLoadoutDisplayPawn* UAFLW_FrontEndMarket::GetDisplayPawn() const
{
	UWorld* World = GetWorld();
	return World ? Cast<AAFLLoadoutDisplayPawn>(UGameplayStatics::GetActorOfClass(World, AAFLLoadoutDisplayPawn::StaticClass())) : nullptr;
}

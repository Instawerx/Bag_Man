// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_FrontEndMarket.h"

#include "AFLCombat.h"                 // LogAFLCombat
#include "AFLCosmeticCoreTypes.h"      // FAFLCatalogEntry
#include "CommonButtonBase.h"          // UCommonButtonBase (the store's tabs; rewired in LOADOUT)
#include "Cosmetics/AFLCharacterPartMap.h"      // UAFLCharacterPartMap (IDENTITY id -> robot class)
#include "Cosmetics/AFLCosmeticSelectionTypes.h" // FAFLCosmeticSelection::GetActiveIdentityId
#include "Components/Border.h"         // UBorder (backdrop brushes)
#include "Components/ListView.h"       // UListView (SetListItems, OnGetEntryClassForItem, OnEntryWidgetGenerated)
#include "Components/TextBlock.h"      // relabel the tab captions + wallet pill values
#include "Components/Widget.h"         // UWidget + GetWidgetFromName result
#include "Components/Border.h"         // CHROME: neon-outline wallet pills
#include "Components/Image.h"          // CHROME: coin-icon tints
#include "Styling/SlateBrush.h"        // CHROME: RoundedBox pill brush
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Cosmetics/AFLWalletComponent.h" // B2c: ClientRequestPurchase + OnWalletChanged (buy + live OWNED flip)
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Input/CommonUIInputTypes.h"  // FUIInputConfig, ECommonInputMode, EMouseCaptureMode
#include "Kismet/GameplayStatics.h"    // UGameplayStatics::GetActorOfClass
#include "UI/AFLCosmeticBrowserLibrary.h" // owned feed + equip + fan-out
#include "UI/AFLLoadoutDisplayPawn.h"  // AAFLLoadoutDisplayPawn
#include "UI/AFLW_LoadoutTileBase.h"   // UAFLW_LoadoutTileBase + UAFLMarketLoadoutItem
#include "UObject/UnrealType.h"        // FNameProperty / CastField -- read CosmeticId off the store's BP item

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

	// LOADOUT overrides the list with OUR tile. STORE now also runs a light C++ pass -- the category tabs shipped
	// with NO click binding, so we bind them to FILTER the store's own items (tile + buy untouched). The LOADOUT
	// instance transiently hits EnterStoreMode too (Mode is still Store at construct; the push init-hook flips it
	// after), but EnterLoadoutMode then re-binds the tabs + repopulates, so that store pass is harmlessly replaced.
	if (Mode == EAFLMarketMode::Loadout)
	{
		EnterLoadoutMode();
	}
	else
	{
		EnterStoreMode();
	}
}

void UAFLW_FrontEndMarket::NativeDestruct()
{
	// Leaving the market -> drop any active store preview so the display robot isn't left wearing an unbought item.
	RevertStorePreview();
	Super::NativeDestruct();
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

	// LEFT browser rail -> solid gloss-black glass (matches the product cards' #05080F) so the armory does NOT bleed
	// through it -- only the CENTER dissolves to the live hero, the RIGHT detail panel keeps its scene window. First
	// matching Border wins; the log confirms which name hit (adjust if none do).
	for (const TCHAR* RailName : { TEXT("LeftBrowser"), TEXT("LeftBrowserPanel"), TEXT("BrowserPanel"), TEXT("LeftRail"), TEXT("ShopRail"), TEXT("LeftColumn"), TEXT("ShopPanel") })
	{
		if (UBorder* Rail = Cast<UBorder>(GetWidgetFromName(FName(RailName))))
		{
			Rail->SetBrushColor(FLinearColor(0.0196f, 0.0314f, 0.0588f, 0.93f)); // #05080F gloss-black @ 0.93
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: LEFT rail '%s' -> gloss-black."), RailName);
			break;
		}
	}
}

// ===================================================================================================
//  STEP A -- STORE mode: make the category tabs FILTER. Path 1 -- filter the store's OWN BP items
//  (BP_AFL_StoreEntryData) by CosmeticId namespace; the store's tile + BUY/EQUIP + BP graph stay intact.
//  The tabs shipped as UBorders with NO click binding (that's why they were dead) -> we bind
//  OnMouseButtonDownEvent, the same single-cast mechanism LOADOUT uses.
// ===================================================================================================

namespace
{
	// Each store tab -> the CosmeticId namespace prefix(es) it shows. The catalog's EAFLCosmeticType values don't
	// map 1:1 to the six marketing tabs, so this is the (operator-tunable) taxonomy: change a prefix and rebuild.
	// A tab whose prefixes match nothing in the catalog simply shows empty. The trailing '.' matters -- it stops
	// "AFL.Weapon." from also swallowing "AFL.WeaponSkin.".
	struct FStoreTabDef { const TCHAR* TabName; const TCHAR* LabelName; const TCHAR* Prefixes[3]; };
	// TAXONOMY (operator-ruled): each tab holds what its label says. EAFLCosmeticType doesn't map 1:1 to the six
	// tabs, so this IS the taxonomy. Two tabs are REPURPOSED -- the widget NAME is kept, only the caption is
	// relabeled (in EnterStoreMode): Tab_HELMETS -> "CAMOS" (weapon-skins, all NeonCamo), Tab_EMOTES -> "BEAMS".
	// Identities aren't sold (free/earned -> loadout), so no store tab holds them.
	static const FStoreTabDef GStoreTabs[6] = {
		{ TEXT("Tab_WEAPONS"), TEXT("TabLabel_WEAPONS"), { TEXT("AFL.Weapon."),     TEXT("AFL.Ability."), nullptr           } },
		{ TEXT("Tab_SKINS"),   TEXT("TabLabel_SKINS"),   { TEXT("AFL.Finish."),     TEXT("AFL.Body."),    TEXT("AFL.Edge.")  } },
		{ TEXT("Tab_HELMETS"), TEXT("TabLabel_HELMETS"), { TEXT("AFL.WeaponSkin."), nullptr,              nullptr           } }, // -> CAMOS
		{ TEXT("Tab_VISORS"),  TEXT("TabLabel_VISORS"),  { TEXT("AFL.Facemask."),   nullptr,              nullptr           } },
		{ TEXT("Tab_EMOTES"),  TEXT("TabLabel_EMOTES"),  { TEXT("AFL.Beam."),       nullptr,              nullptr           } }, // -> BEAMS
		{ TEXT("Tab_BUNDLES"), TEXT("TabLabel_BUNDLES"), { TEXT("AFL.Bundle."),     nullptr,              nullptr           } },
	};
}

void UAFLW_FrontEndMarket::EnterStoreMode()
{
	// Bind the six category tabs. Mirror LOADOUT's PrepTab: the store tabs are UBorder (not buttons), so their
	// click is OnMouseButtonDownEvent (single-cast dynamic). They had NO prior binding -> ours is the only one.
	// BindDynamic needs the literal function token, so we can't loop this.
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
	if (UBorder* B = PrepTab(TEXT("Tab_WEAPONS"))) { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnStoreTabWeapons); }
	if (UBorder* B = PrepTab(TEXT("Tab_SKINS")))   { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnStoreTabSkins); }
	if (UBorder* B = PrepTab(TEXT("Tab_HELMETS"))) { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnStoreTabHelmets); }
	if (UBorder* B = PrepTab(TEXT("Tab_VISORS")))  { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnStoreTabVisors); }
	if (UBorder* B = PrepTab(TEXT("Tab_EMOTES")))  { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnStoreTabEmotes); }
	if (UBorder* B = PrepTab(TEXT("Tab_BUNDLES"))) { B->OnMouseButtonDownEvent.BindDynamic(this, &UAFLW_FrontEndMarket::OnStoreTabBundles); }

	// TAXONOMY: relabel the two repurposed tabs (the widget names stay Tab_HELMETS/Tab_EMOTES; only the caption
	// changes). CAMOS = weapon-skins (all NeonCamo, so the label reads true); BEAMS = the beam VFX (was EMOTES).
	if (UTextBlock* L = Cast<UTextBlock>(GetWidgetFromName(TEXT("TabLabel_HELMETS")))) { L->SetText(FText::FromString(TEXT("CAMOS"))); }
	if (UTextBlock* L = Cast<UTextBlock>(GetWidgetFromName(TEXT("TabLabel_EMOTES")))) { L->SetText(FText::FromString(TEXT("BEAMS"))); }

	// STORE PREVIEW: bind the ListView's NATIVE selection event (the BP one is private) so selecting a card
	// previews it on the display robot. AddUObject (native multicast) is additive -> the store's own BP selection
	// handler (details panel) is untouched. Deselect / tab-change / close revert it.
	if (UListView* List = Cast<UListView>(GetWidgetFromName(TEXT("ShopListView"))))
	{
		List->OnItemSelectionChanged().AddUObject(this, &UAFLW_FrontEndMarket::OnStoreItemSelectionChanged);

		// STORE's OWN TILE: point the ListView at OUR readable tile over the store's SAME BP items. The store BP
		// tile's BUY/EQUIP buttons ATE the row click (you could never browse past the auto-first item -- a store you
		// can't navigate is broken). OUR tile's SelectButton is a delegate WE own, so a body-click reliably selects
		// the row -> the store's detail panel + BUY AND our try-on preview fire (HandleStoreTileClicked). The store's
		// BP ITEMS stay the list data, so the BP detail panel + buy read them exactly as before -- zero buy-spine risk.
		if (!LoadoutTileClass)
		{
			LoadoutTileClass = LoadClass<UAFLW_LoadoutTileBase>(nullptr,
				TEXT("/Game/BagMan/UI/Loadout/WBP_AFL_LoadoutTile.WBP_AFL_LoadoutTile_C"));
		}
		List->OnGetEntryClassForItem().BindUObject(this, &UAFLW_FrontEndMarket::GetLoadoutEntryClass);
		List->OnEntryWidgetGenerated().AddUObject(this, &UAFLW_FrontEndMarket::OnStoreTileGenerated);
		List->RegenerateAllEntries(); // the store BP generated BP tiles on construct -> rebuild them as OURS
	}

	// B2c: a per-tile BUY must flip that tile BUY -> OWNED/EQUIP live -> bind the wallet's change signal to a refresh.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APlayerState* PS = PC->PlayerState)
		{
			if (UAFLWalletComponent* Wallet = PS->FindComponentByClass<UAFLWalletComponent>())
			{
				Wallet->OnWalletChanged.AddUniqueDynamic(this, &UAFLW_FrontEndMarket::OnStoreWalletChanged);
				RefreshWalletChrome(Wallet->GetVolts(), Wallet->GetWatts()); // CHROME: seed the pills with the current balance
			}
		}
	}
	StyleChrome(); // CHROME: one-time SSOT styling -- pills + profile chip + bottom utility bar (safe if any widget is absent)

	// Show ALL purchasables initially (no regression vs today); the first tab click applies a filter + the cyan
	// active cue. We DON'T recolor the tabs here, so the store's shipped look is untouched until the user filters.
	ActiveStoreTab = -1;
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: EnterStoreMode -> 6 category tabs wired to filter."));
}

FName UAFLW_FrontEndMarket::ReadEntryCosmeticId(const UObject* Item)
{
	if (!Item)
	{
		return NAME_None;
	}
	// The store item is a BP UObject (BP_AFL_StoreEntryData) with a confirmed 'CosmeticId' FName property.
	if (const FNameProperty* Prop = CastField<FNameProperty>(Item->GetClass()->FindPropertyByName(TEXT("CosmeticId"))))
	{
		return Prop->GetPropertyValue_InContainer(Item);
	}
	return NAME_None;
}

void UAFLW_FrontEndMarket::FilterStore(int32 TabIndex)
{
	if (TabIndex < 0 || TabIndex >= 6)
	{
		return;
	}
	// Switching category drops any active preview -> the robot returns to the real loadout before the new list.
	RevertStorePreview();
	UListView* List = Cast<UListView>(GetWidgetFromName(TEXT("ShopListView")));
	if (!List)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MARKET: FilterStore - 'ShopListView' not found / not a UListView."));
		return;
	}

	// Lazily cache the store's FULL item set (the store BP populated it on construct). Refresh if the store grew
	// the list (e.g. a post-purchase re-populate) so we always filter from the complete set.
	const TArray<UObject*>& Current = List->GetListItems();
	if (StoreFullItems.Num() == 0 || Current.Num() > StoreFullItems.Num())
	{
		StoreFullItems.Reset(Current.Num());
		for (UObject* O : Current)
		{
			StoreFullItems.Add(O);
		}

		// Taxonomy census: log the distinct "AFL.<Type>." namespaces + counts in the store's full set, so the
		// tab->category table above can be tuned to the REAL catalog from a single PIE (no guessing/rebuild loop).
		TMap<FString, int32> NsCount;
		for (const TObjectPtr<UObject>& Obj : StoreFullItems)
		{
			const FString IdStr = ReadEntryCosmeticId(Obj.Get()).ToString();
			TArray<FString> Parts;
			IdStr.ParseIntoArray(Parts, TEXT("."));
			const FString Ns = (Parts.Num() >= 2) ? (Parts[0] + TEXT(".") + Parts[1] + TEXT(".")) : IdStr;
			NsCount.FindOrAdd(Ns)++;
		}
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: store taxonomy census -- %d item(s), %d namespace(s):"), StoreFullItems.Num(), NsCount.Num());
		for (const TPair<FString, int32>& Pair : NsCount)
		{
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET:   %-24s x%d"), *Pair.Key, Pair.Value);
		}
	}

	const FStoreTabDef& Def = GStoreTabs[TabIndex];
	TArray<UObject*> Subset;
	Subset.Reserve(StoreFullItems.Num());
	for (const TObjectPtr<UObject>& Obj : StoreFullItems)
	{
		const FString IdStr = ReadEntryCosmeticId(Obj.Get()).ToString();
		for (const TCHAR* Pfx : Def.Prefixes)
		{
			if (Pfx && IdStr.StartsWith(Pfx, ESearchCase::IgnoreCase))
			{
				Subset.Add(Obj.Get());
				break;
			}
		}
	}

	// Reuse the store's OWN item objects -> its BP tile renders + buys them exactly as before, just a subset.
	List->SetListItems(Subset);
	ActiveStoreTab = TabIndex;
	UpdateStoreTabVisuals(TabIndex);

	// AUTO-PREVIEW (ruled, reference-correct): open every tab on its HERO product -- auto-select the first item so
	// the robot instantly shows it (armed for weapon/camo/beam via the auto-arm; colored/visored for the rest, NO
	// gun on color axes) AND the store's own detail panel populates (name/series/rarity/price). This fires the
	// SAME OnItemSelectionChanged chain a manual body-click would, so a later body-click just REFINES it -- auto-
	// first is the default, not a lock. SetListItems cleared the selection above, so this always re-fires. Empty
	// tab -> nothing selected, robot stays clean (RevertStorePreview ran at the top of FilterStore). No stacking:
	// the top-of-function revert (empty tabs) + ApplyPreview's re-seed-from-committed (non-empty) both prevent it.
	// Select the first PREVIEWABLE item (skip non-visual entries -- e.g. the EMP ability in WEAPONS -- so a tab
	// whose first row has no visual preview still opens on a shown product).
	UObject* AutoItem = nullptr;
	for (UObject* Obj : Subset)
	{
		EAFLLoadoutAxis Ax;
		if (ClassifyStoreAxis(ReadEntryCosmeticId(Obj), Ax)) { AutoItem = Obj; break; }
	}
	if (AutoItem)
	{
		List->SetSelectedItem(AutoItem);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: store tab '%s' -> %d/%d item(s) (auto-preview=%s)."),
		Def.LabelName, Subset.Num(), StoreFullItems.Num(),
		AutoItem ? *ReadEntryCosmeticId(AutoItem).ToString() : TEXT("<none>"));
}

void UAFLW_FrontEndMarket::UpdateStoreTabVisuals(int32 ActiveIndex)
{
	// Active tab -> cyan label + a faint cyan fill; inactive -> dim label + transparent fill. (A true underline
	// bar needs a dedicated per-tab widget; label-cyan is the robust active cue over the store's shipped tabs.)
	static const FSlateColor CyanActive(FLinearColor(0.15f, 0.95f, 1.0f, 1.0f));
	static const FSlateColor DimInactive(FLinearColor(0.62f, 0.68f, 0.80f, 1.0f));
	for (int32 i = 0; i < 6; ++i)
	{
		if (UTextBlock* L = Cast<UTextBlock>(GetWidgetFromName(FName(GStoreTabs[i].LabelName))))
		{
			L->SetColorAndOpacity(i == ActiveIndex ? CyanActive : DimInactive);
		}
		if (UBorder* B = Cast<UBorder>(GetWidgetFromName(FName(GStoreTabs[i].TabName))))
		{
			B->SetBrushColor(i == ActiveIndex ? FLinearColor(0.05f, 0.55f, 0.72f, 0.30f) : FLinearColor(0.f, 0.f, 0.f, 0.f));
		}
	}
}

// One tiny handler per tab -- OnMouseButtonDownEvent is a single-cast dynamic (no capture), so we can't share one.
#define AFL_STORE_TAB_HANDLER(FnName, Index) \
	FEventReply UAFLW_FrontEndMarket::FnName(FGeometry, const FPointerEvent&) \
	{ FilterStore(Index); return FEventReply(true); }
AFL_STORE_TAB_HANDLER(OnStoreTabWeapons, 0)
AFL_STORE_TAB_HANDLER(OnStoreTabSkins,   1)
AFL_STORE_TAB_HANDLER(OnStoreTabHelmets, 2)
AFL_STORE_TAB_HANDLER(OnStoreTabVisors,  3)
AFL_STORE_TAB_HANDLER(OnStoreTabEmotes,  4)
AFL_STORE_TAB_HANDLER(OnStoreTabBundles, 5)
#undef AFL_STORE_TAB_HANDLER

// --- STORE PREVIEW (front-end try-before-buy) -- selecting a store card shows it on the display robot ---

bool UAFLW_FrontEndMarket::ClassifyStoreAxis(FName CosmeticId, EAFLLoadoutAxis& OutAxis)
{
	const FString Id = CosmeticId.ToString();
	// Order matters: test "AFL.WeaponSkin." BEFORE "AFL.Weapon." (the trailing '.' keeps them distinct anyway).
	if (Id.StartsWith(TEXT("AFL.WeaponSkin."), ESearchCase::IgnoreCase)) { OutAxis = EAFLLoadoutAxis::WeaponSkin; return true; }
	if (Id.StartsWith(TEXT("AFL.Weapon."),     ESearchCase::IgnoreCase)) { OutAxis = EAFLLoadoutAxis::Weapon;     return true; }
	if (Id.StartsWith(TEXT("AFL.Finish."),     ESearchCase::IgnoreCase) ||
	    Id.StartsWith(TEXT("AFL.Body."),       ESearchCase::IgnoreCase)) { OutAxis = EAFLLoadoutAxis::BodyColor;  return true; }
	if (Id.StartsWith(TEXT("AFL.Edge."),       ESearchCase::IgnoreCase)) { OutAxis = EAFLLoadoutAxis::EdgeColor;  return true; }
	if (Id.StartsWith(TEXT("AFL.Beam."),       ESearchCase::IgnoreCase)) { OutAxis = EAFLLoadoutAxis::Beam;       return true; }
	if (Id.StartsWith(TEXT("AFL.Facemask."),   ESearchCase::IgnoreCase)) { OutAxis = EAFLLoadoutAxis::Facemask;   return true; }
	if (Id.StartsWith(TEXT("AFL.Character."),  ESearchCase::IgnoreCase) ||
	    Id.StartsWith(TEXT("AFL.Team."),       ESearchCase::IgnoreCase)) { OutAxis = EAFLLoadoutAxis::Identity;   return true; }
	return false; // Bundles / unknown -> no single preview axis
}

void UAFLW_FrontEndMarket::OnStoreItemSelectionChanged(UObject* Item)
{
	// [PREVIEW-DIAG] step a -- did the native selection event fire at all? If this line NEVER appears when you click
	// a store tile, the tile's own BUY/EQUIP buttons swallow the click / the row never selects (the opaque-BP-tile
	// trap -- exactly what killed the loadout's OnItemClicked). That would be the break; report it.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: [PREVIEW a] selection FIRED item=%s mode=%d."),
		Item ? *Item->GetName() : TEXT("<null>"), (int32)Mode);

	// STORE mode only (LOADOUT drives the list with OUR tile + its own click; this bind is inert there via the guard).
	if (Mode != EAFLMarketMode::Store)
	{
		return;
	}
	if (!Item)
	{
		RevertStorePreview(); // deselect -> back to the real loadout
		return;
	}

	const FName CosmeticId = ReadEntryCosmeticId(Item);
	EAFLLoadoutAxis Axis = EAFLLoadoutAxis::BodyColor;
	const bool bClassified = !CosmeticId.IsNone() && ClassifyStoreAxis(CosmeticId, Axis);
	// [PREVIEW-DIAG] steps b + c -- item class, CosmeticId read off the BP item, and axis classification.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: [PREVIEW b/c] itemClass=%s cosmeticId=%s classified=%s axis=%d."),
		*Item->GetClass()->GetName(), *CosmeticId.ToString(), bClassified ? TEXT("YES") : TEXT("NO"), (int32)Axis);
	if (!bClassified)
	{
		return; // e.g. a bundle -> nothing single-axis to preview
	}

	APlayerController* PC = GetOwningPlayer();
	AAFLLoadoutDisplayPawn* Disp = GetDisplayPawn();
	// [PREVIEW-DIAG] step d -- controller + display pawn resolved, about to call ApplyPreview.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: [PREVIEW d] PC=%d disp=%d -> ApplyPreview(%s, axis=%d)."),
		PC != nullptr, Disp != nullptr, *CosmeticId.ToString(), (int32)Axis);
	if (!PC || !Disp)
	{
		return;
	}
	// PREVIEW (no commit -> the entitlement gate is bypassed; unowned try-before-buy works).
	UAFLCosmeticBrowserLibrary::ApplyPreview(PC, Disp, Axis, CosmeticId);
}

void UAFLW_FrontEndMarket::RevertStorePreview()
{
	APlayerController* PC = GetOwningPlayer();
	AAFLLoadoutDisplayPawn* Disp = GetDisplayPawn();
	if (PC && Disp)
	{
		UAFLCosmeticBrowserLibrary::RevertToSaved(PC, Disp);
	}
}

void UAFLW_FrontEndMarket::OnStoreTileGenerated(UUserWidget& EntryWidget)
{
	// Bind each generated OUR-tile's own click. On a LOADOUT instance this bind also runs (EnterStoreMode fires
	// transiently before the Mode flip), but HandleStoreTileClicked guards Mode, so it is inert there. AddUnique so
	// pooled/regenerated tiles never stack bindings.
	if (UAFLW_LoadoutTileBase* Tile = Cast<UAFLW_LoadoutTileBase>(&EntryWidget))
	{
		Tile->OnTileClicked.AddUniqueDynamic(this, &UAFLW_FrontEndMarket::HandleStoreTileClicked);
		Tile->OnBuyClicked.AddUniqueDynamic(this, &UAFLW_FrontEndMarket::HandleStoreTileBuy);
		Tile->OnEquipClicked.AddUniqueDynamic(this, &UAFLW_FrontEndMarket::HandleStoreTileEquip);
	}
}

void UAFLW_FrontEndMarket::HandleStoreTileBuy(FName CosmeticId, bool bWatts)
{
	if (Mode != EAFLMarketMode::Store) { return; }
	APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	if (!Wallet)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MARKET: [BUY] no wallet on PlayerState."));
		return;
	}
	const EAFLPayCurrency Pay = bWatts ? EAFLPayCurrency::Watts : EAFLPayCurrency::Volts;
	// Watts-wall + double-charge + afford are all guarded inside the wallet; OnWalletChanged -> OnStoreWalletChanged
	// refreshes the tiles (BUY -> OWNED flip).
#if UE_BUILD_SHIPPING
	// SHIPPING: PlayFab-backed purchase -- requires the cosmetic to be seeded in the AFL_Main catalog.
	Wallet->ClientRequestPurchase(CosmeticId, Pay);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: [BUY] %s pay=%s -> ClientRequestPurchase (PlayFab)."),
		*CosmeticId.ToString(), bWatts ? TEXT("Watts") : TEXT("Volts"));
#else
	// DEV/PIE: local-authority grant (the path the legacy store BP + cheats use). ClientRequestPurchase would hit
	// PlayFab, whose AFL_Main catalog holds only the 3 test items -> every real cosmetic returns ItemNotFound. This
	// grants locally so the buy loop (afford / Watts-wall / double-charge guard / OWNED-flip) is exercised in PIE.
	Wallet->ServerPurchaseCosmetic(CosmeticId, Pay);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: [BUY] %s pay=%s -> ServerPurchaseCosmetic (dev grant)."),
		*CosmeticId.ToString(), bWatts ? TEXT("Watts") : TEXT("Volts"));
#endif
}

void UAFLW_FrontEndMarket::HandleStoreTileEquip(FName CosmeticId)
{
	if (Mode != EAFLMarketMode::Store) { return; }
	// Owned item -> try it on the hero (store = preview; the loadout is where a commit lives).
	EAFLLoadoutAxis Axis = EAFLLoadoutAxis::BodyColor;
	if (!ClassifyStoreAxis(CosmeticId, Axis)) { return; }
	APlayerController* PC = GetOwningPlayer();
	AAFLLoadoutDisplayPawn* Disp = GetDisplayPawn();
	if (PC && Disp)
	{
		UAFLCosmeticBrowserLibrary::ApplyPreview(PC, Disp, Axis, CosmeticId);
	}
}

void UAFLW_FrontEndMarket::OnStoreWalletChanged(int32 Volts, int32 Watts)
{
	// Chrome pills are always visible (both Store + Loadout) -> keep the live balances current regardless of mode.
	RefreshWalletChrome(Volts, Watts);
	// A purchase landed -> re-generate the store tiles so the bought item flips BUY -> OWNED/EQUIP live.
	if (Mode != EAFLMarketMode::Store) { return; }
	if (UListView* List = Cast<UListView>(GetWidgetFromName(TEXT("ShopListView"))))
	{
		List->RegenerateAllEntries();
	}
}

namespace
{
	static const FLinearColor GAFLNeonBlue(0.1176f, 0.3529f, 1.0f, 1.0f);    // #1E5AFF Volts
	static const FLinearColor GAFLNeonMagenta(1.0f, 0.1765f, 0.6196f, 1.0f); // #FF2D9E Watts

	// CHROME pill: dark gloss glass fill + a bright neon outline, fully-rounded ends (HalfHeightRadius). Same
	// gloss-black + neon-accent language as the product cards, so the whole store reads as one surface.
	FSlateBrush MakeNeonPillBrush(const FLinearColor& Neon)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(FLinearColor(0.0196f, 0.0314f, 0.0588f, 0.85f)); // gloss-black glass
		B.OutlineSettings.Color = FSlateColor(Neon);
		B.OutlineSettings.Width = 1.5f;
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius; // pill ends
		return B;
	}
}

void UAFLW_FrontEndMarket::RefreshWalletChrome(int32 Volts, int32 Watts)
{
	if (UTextBlock* V = Cast<UTextBlock>(GetWidgetFromName(TEXT("VoltsValue")))) { V->SetText(FText::AsNumber(Volts)); }
	if (UTextBlock* W = Cast<UTextBlock>(GetWidgetFromName(TEXT("WattsValue")))) { W->SetText(FText::AsNumber(Watts)); }
	// The scaffold's old single-line readout (WalletText) duplicated the pills' amounts -> hide it. Re-collapsed on
	// every refresh so a store-BP re-show can't bring the duplicate back.
	if (UWidget* Old = GetWidgetFromName(TEXT("WalletText"))) { Old->SetVisibility(ESlateVisibility::Collapsed); }
}

void UAFLW_FrontEndMarket::StyleChrome()
{
	// --- TOP: WALLET PILLS -- dark-gloss + neon outline. Cast-guarded; a non-UBorder pill still shows its value. ---
	if (UBorder* Pill = Cast<UBorder>(GetWidgetFromName(TEXT("VoltsPill")))) { Pill->SetBrush(MakeNeonPillBrush(GAFLNeonBlue)); }
	else { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MARKET: chrome - 'VoltsPill' not a UBorder (outline skipped).")); }
	if (UBorder* Pill = Cast<UBorder>(GetWidgetFromName(TEXT("WattsPill")))) { Pill->SetBrush(MakeNeonPillBrush(GAFLNeonMagenta)); }
	else { UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MARKET: chrome - 'WattsPill' not a UBorder (outline skipped).")); }
	if (UImage* Coin = Cast<UImage>(GetWidgetFromName(TEXT("VoltsCoin")))) { Coin->SetColorAndOpacity(GAFLNeonBlue); }
	if (UImage* Coin = Cast<UImage>(GetWidgetFromName(TEXT("WattsCoin")))) { Coin->SetColorAndOpacity(GAFLNeonMagenta); }
	if (UTextBlock* V = Cast<UTextBlock>(GetWidgetFromName(TEXT("VoltsValue")))) { V->SetColorAndOpacity(FSlateColor(FLinearColor::White)); }
	if (UTextBlock* W = Cast<UTextBlock>(GetWidgetFromName(TEXT("WattsValue")))) { W->SetColorAndOpacity(FSlateColor(FLinearColor::White)); }

	// --- TOP: PROFILE CHIP -- name is REAL (PlayerState); level/XP are a STUB ("LVL --"), no progression system yet.
	// The scaffold has only ProfileText (no avatar Image / XP-bar widget) -> those are deferred, not built here. ---
	if (UTextBlock* Prof = Cast<UTextBlock>(GetWidgetFromName(TEXT("ProfileText"))))
	{
		FString Name = TEXT("PLAYER");
		if (const APlayerController* PC = GetOwningPlayer())
		{
			if (const APlayerState* PS = PC->PlayerState)
			{
				const FString N = PS->GetPlayerName();
				if (!N.IsEmpty()) { Name = N.ToUpper(); }
			}
		}
		// Separator kept ASCII (" - ") -- a raw UTF-8 bullet in a wide literal can garble under MSVC without /utf-8.
		Prof->SetText(FText::FromString(FString::Printf(TEXT("%s  -  LVL --"), *Name)));
		Prof->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}

	// --- BOTTOM: UTILITY BAR -- dark-gloss band + tinted labels. CloseButton already closes the menu; UtilGear /
	// UtilHelp / UtilSocial / UtilNotif are VISUAL STUBS (no system behind them yet -- flagged, not wired). ---
	if (UBorder* Bar = Cast<UBorder>(GetWidgetFromName(TEXT("UtilityBar")))) { Bar->SetBrushColor(FLinearColor(0.0196f, 0.0314f, 0.0588f, 0.85f)); }
	const FLinearColor UtilTint(0.55f, 0.78f, 0.92f, 1.0f); // muted cyan -- reads on the dark band
	for (const TCHAR* N : { TEXT("CloseLabel"), TEXT("UtilHelp"), TEXT("UtilSocial"), TEXT("UtilNotif"), TEXT("UtilGear") })
	{
		if (UTextBlock* T = Cast<UTextBlock>(GetWidgetFromName(N))) { T->SetColorAndOpacity(FSlateColor(UtilTint)); }
	}
}

void UAFLW_FrontEndMarket::HandleStoreTileClicked(EAFLLoadoutAxis /*Axis*/, FName CosmeticId)
{
	// LOADOUT drives equip via HandleLoadoutTileClicked; this bind is inert there.
	if (Mode != EAFLMarketMode::Store)
	{
		return;
	}
	// Drive a REAL ListView selection for the clicked id. SetSelectedItem fires BOTH the native OnItemSelectionChanged
	// (-> OnStoreItemSelectionChanged -> our try-on preview) AND the store's own BP selection handler (-> its detail
	// panel + BUY). One body-click, both surfaces -- the reliable browse that the BP tile's click-eating buttons broke.
	UListView* List = Cast<UListView>(GetWidgetFromName(TEXT("ShopListView")));
	if (!List)
	{
		return;
	}
	for (UObject* Item : List->GetListItems())
	{
		if (ReadEntryCosmeticId(Item) == CosmeticId)
		{
			List->SetSelectedItem(Item);
			break;
		}
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: [STORE tile] click %s -> SetSelectedItem (detail panel + BUY + preview)."),
		*CosmeticId.ToString());
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

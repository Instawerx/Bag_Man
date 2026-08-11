// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_VenueShowcase.h"

#include "AFLCombat.h"              // LogAFLCombat
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Containers/Ticker.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_VenueShowcase)

#define LOCTEXT_NAMESPACE "AFLVenueShowcase"

namespace
{
	/** `afl.Venues.Select <index>` -- drive the showcase without an input device. */
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLVenuesSelectCmd(
		TEXT("afl.Venues.Select"),
		TEXT("Select a venue in the S8 showcase by index: afl.Venues.Select [0..N]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				const int32 Index = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				TWeakObjectPtr<UWorld> WeakWorld(World);
				double Deadline = 25.0;

				// Waits for the screen, like every probe in this front end: -ExecCmds fires at engine init
				// and a headless session has no mouse.
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
					[Index, WeakWorld, Deadline](float Delta) mutable -> bool
					{
						Deadline -= Delta;
						UWorld* W = WeakWorld.Get();
						if (!W || Deadline <= 0.0)
						{
							UE_LOG(LogAFLCombat, Error, TEXT("AFL_VENUES: afl.Venues.Select gave up -- no showcase."));
							return false;
						}
						for (TObjectIterator<UAFLW_VenueShowcase> It; It; ++It)
						{
							UAFLW_VenueShowcase* S = *It;
							if (S && S->GetWorld() == W && !S->HasAnyFlags(RF_ClassDefaultObject))
							{
								S->SelectVenue(Index);
								return false;
							}
						}
						return true;
					}), 0.5f);

				Ar.Logf(TEXT("afl.Venues.Select -- will select %d once the showcase exists."), Index);
			}));
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  THE §8 INVARIANT
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

bool UAFLW_VenueShowcase::IsExitLegal(FName CarriedVenue, int64 CarriedStake)
{
	// A route out of the showcase carries NEITHER a venue NOR a stake.
	//
	// The venue half is §8: "never pre-filtered by venue, because a pre-filter is a venue choice wearing a
	// different name". The stake half is R98 arriving from the other direction -- this surface sits behind
	// a footer item every player can reach, including the majority who are never routed through a wagering
	// surface at all, so a stake leaving here would put a buy-in on the free half of the economy.
	return CarriedVenue.IsNone() && CarriedStake == 0;
}

FName UAFLW_VenueShowcase::BuildLobbyDeepLink()
{
	// NAME_None BY CONSTRUCTION, not by choice at the call site. The selected venue is not a parameter, so
	// leaking it would require changing this signature -- which is exactly the edit the test is watching
	// for. A function that COULD return a venue and merely chooses not to is one refactor from doing it.
	return NAME_None;
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  LIFECYCLE
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

void UAFLW_VenueShowcase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (BackButton)
	{
		BackButton->OnClicked().AddWeakLambda(this, [this] { DeactivateWidget(); });
	}
	if (LobbyLink)
	{
		LobbyLink->OnClicked().AddWeakLambda(this, [this]
		{
			// Pops back to whatever pushed this -- the home screen. NOT a push to a lobby: which door a
			// player wants is the R98 first decision and it is not this surface's to make.
			const FName Carried = BuildLobbyDeepLink();
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_VENUES: lobby link taken, carrying venue='%s' (must be None)."),
				Carried.IsNone() ? TEXT("None") : *Carried.ToString());
			DeactivateWidget();
		});
	}

	RebuildTiles();
}

void UAFLW_VenueShowcase::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Rebuilt per activation for the same reason every other surface here re-applies: the venue list is
	// authored data that can change between two visits without the widget being reconstructed.
	RebuildTiles();
	SelectVenue(Venues.IsValidIndex(SelectedIndex) ? SelectedIndex : 0);
}

UWidget* UAFLW_VenueShowcase::NativeGetDesiredFocusTarget() const
{
	if (Tiles.IsValidIndex(0) && Tiles[0])
	{
		return Tiles[0];
	}
	return Super::NativeGetDesiredFocusTarget();
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  LIST + DETAIL
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

void UAFLW_VenueShowcase::RebuildTiles()
{
	if (!VenueList || !TileClass)
	{
		return;   // a WBP may author its own tiles; nothing to do
	}

	VenueList->ClearChildren();
	Tiles.Reset();

	for (int32 Index = 0; Index < Venues.Num(); ++Index)
	{
		const FAFLVenueEntry& Venue = Venues[Index];
		if (!Venue.IsValid())
		{
			// Loud rather than skipped-in-silence: a half-authored row is a blank tile in a surface whose
			// entire job is presentation, and a blank tile reads as a broken map rather than a bad entry.
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_VENUES: venue entry %d is incomplete (needs DisplayName and MapId) -- skipped."), Index);
			continue;
		}

		UAFLW_VenueTile* Tile = WidgetTree
			? WidgetTree->ConstructWidget<UAFLW_VenueTile>(TileClass)
			: nullptr;
		if (!Tile)
		{
			continue;
		}
		Tile->SetVenue(Venue);

		const int32 Captured = Index;
		Tile->OnClicked().AddWeakLambda(this, [this, Captured] { SelectVenue(Captured); });

		VenueList->AddChild(Tile);
		Tiles.Add(Tile);
	}

	UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_VENUES: %d tile(s) built from %d entries."), Tiles.Num(), Venues.Num());
}

void UAFLW_VenueShowcase::SelectVenue(int32 Index)
{
	if (!Venues.IsValidIndex(Index))
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_VENUES: no venue at index %d (have %d)."), Index, Venues.Num());
		return;
	}
	SelectedIndex = Index;
	ApplyDetail();

	const FAFLVenueEntry& Venue = Venues[Index];
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_VENUES: showing '%s' (%s, %s)."),
		*Venue.DisplayName.ToString(),
		*UAFLW_VenueTile::FormatVenueClass(Venue.VenueClass).ToString(),
		*Venue.MapId.ToString());

	BP_OnVenueSelected(Venue);
}

void UAFLW_VenueShowcase::ApplyDetail()
{
	if (!Venues.IsValidIndex(SelectedIndex))
	{
		return;
	}
	const FAFLVenueEntry& Venue = Venues[SelectedIndex];

	if (VenueName)  { VenueName->SetText(Venue.DisplayName); }
	if (VenueBlurb) { VenueBlurb->SetText(Venue.Blurb); }
	if (VenueClassLabel) { VenueClassLabel->SetText(UAFLW_VenueTile::FormatVenueClass(Venue.VenueClass)); }

	if (VenueArt)
	{
		// Synchronous at selection, like every other soft load in this front end: the soft reference keeps
		// the art out of boot, and by here the player has asked to look at this one.
		if (UTexture2D* Art = Venue.KeyArt.LoadSynchronous())
		{
			VenueArt->SetBrushFromTexture(Art, /*bMatchSize=*/false);
			VenueArt->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			// Hidden rather than left showing the PREVIOUS venue's art, which would attach one map's
			// picture to another map's name -- worse than showing nothing on a surface about identity.
			VenueArt->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  TILE
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

FText UAFLW_VenueTile::FormatVenueClass(EAFLVenueClass InClass)
{
	// R97's two classes, in the words the registry uses. Static and shared with the detail panel so one
	// venue cannot be an "ARENA" in the list and something else in the detail.
	return InClass == EAFLVenueClass::Arena
		? LOCTEXT("VenueArena", "ARENA")
		: LOCTEXT("VenueMap",   "MAP");
}

void UAFLW_VenueTile::SetVenue(const FAFLVenueEntry& InVenue)
{
	if (TileName)       { TileName->SetText(InVenue.DisplayName); }
	if (TileClassLabel) { TileClassLabel->SetText(FormatVenueClass(InVenue.VenueClass)); }
	if (TileArt)
	{
		if (UTexture2D* Art = InVenue.KeyArt.LoadSynchronous())
		{
			TileArt->SetBrushFromTexture(Art, /*bMatchSize=*/false);
			TileArt->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			TileArt->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

#undef LOCTEXT_NAMESPACE

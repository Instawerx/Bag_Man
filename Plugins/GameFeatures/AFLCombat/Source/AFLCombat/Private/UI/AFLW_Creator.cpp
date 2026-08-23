// CC-5.2 -- the creator widget's behaviour layer. See AFLW_Creator.h for why the rail is data.
#include "UI/AFLW_Creator.h"

#include "UI/AFLW_LoadoutBase.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "AFLCombat.h"                  // LogAFLCombat -- this file never logged before
#include "Components/Button.h"
#include "Components/EditableTextBox.h"   // the build name the player types
#include "Components/PanelWidget.h"       // the rail the rows are spawned into           // CC-5: the creator's own way out
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_Creator)

namespace
{
	/** The enum's own order IS the rail order. Stable and position-independent, which the CVD rule
	 *  requires -- a player who cannot compare hues navigates by position and label. */
	constexpr EAFLCreatorChannel GRailOrder[] = {
		EAFLCreatorChannel::Body,
		EAFLCreatorChannel::Edge,
		EAFLCreatorChannel::Glow,
		EAFLCreatorChannel::Visor,
	};

	FText ChannelLabel(const EAFLCreatorChannel Ch)
	{
		switch (Ch)
		{
			case EAFLCreatorChannel::Body:  return NSLOCTEXT("AFLCreator", "ChBody",  "Body");
			case EAFLCreatorChannel::Edge:  return NSLOCTEXT("AFLCreator", "ChEdge",  "Edge");
			case EAFLCreatorChannel::Glow:  return NSLOCTEXT("AFLCreator", "ChGlow",  "Glow");
			case EAFLCreatorChannel::Visor: return NSLOCTEXT("AFLCreator", "ChVisor", "Visor");
		}
		return FText::GetEmpty();
	}
}

UAFLW_Creator::UAFLW_Creator()
{
	// ESCAPABLE BY THE FRAMEWORK'S OWN ROUTE. bIsBackHandler is what makes CommonUI deliver the Back
	// action here; without it Escape reached the game viewport and the screen was a dead end.
	bIsBackHandler = true;
}

UWidget* UAFLW_Creator::NativeGetDesiredFocusTarget() const
{
	// A NAME I NEVER VERIFIED IS WHAT TRAPPED THE PLAYER. This asked for "ChannelRail"; the WBP calls it
	// C_ChannelRail. The lookup always missed, Super returned nothing, the widget was not focusable, focus
	// went to the game viewport and Escape did nothing -- twice.
	//
	// So the guarantee no longer depends on a name. Named regions are a PREFERENCE, tried in order and
	// logged when none hit; the widget ITSELF is the last resort, and it is always present.
	static const TCHAR* const Preferred[] = { TEXT("C_ChannelRail"), TEXT("ChannelRail"), TEXT("Root_Shell") };
	for (const TCHAR* Name : Preferred)
	{
		if (UWidget* W = GetWidgetFromName(FName(Name)))
		{
			return W;
		}
	}
	UE_LOG(LogAFLCombat, Verbose,
		TEXT("[Creator] no preferred focus region present -- focusing the creator itself."));
	return const_cast<UAFLW_Creator*>(this);
}

void UAFLW_Creator::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// WITHOUT THIS, RETURNING A FOCUS TARGET IS NOT ENOUGH. The router's own words were "the widget isn't
	// focusable - focusing the game viewport", and a viewport-focused layer never receives Back.
	SetIsFocusable(true);

	// A SECOND, INDEPENDENT WAY OUT. Escape is a keyboard affordance; a pointer-only player needs a
	// control. Optional so the WBP is free to place it, wired here so it cannot be forgotten in Blueprint.
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UAFLW_Creator::HandleCloseClicked);
	}
	if (F_Save)
	{
		F_Save->OnClicked.AddDynamic(this, &UAFLW_Creator::HandleSaveClicked);
	}
	else
	{
		// SAYS SO. An unbound commit button is exactly the "built, correct, unreachable" shape this
		// step exists to end.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[Creator] F_Save not bound -- the creator has no way to commit a build."));
	}
}

void UAFLW_Creator::NativeOnActivated()
{
	Super::NativeOnActivated();

	// ON ACTIVATE, NOT ON CONSTRUCT. The schema resolves against the loadout's DisplayPawn, which may not
	// have spawned when this widget is constructed -- resolving early is how an earlier probe ended up
	// reporting channels against whatever material happened to be reachable.
	RefreshFromSchema();
	RebuildChannelRows();
}

void UAFLW_Creator::RebuildChannelRows()
{
	if (!ChannelRailContainer)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[Creator] no ChannelRailContainer bound -- the rail cannot render. (BindWidgetOptional)"));
		return;
	}
	ChannelRailContainer->ClearChildren();

	if (!ChannelRowClass)
	{
		// SAYS SO RATHER THAN DRAWING NOTHING. An unset row class and a resolved-but-empty schema are
		// indistinguishable on screen, and that ambiguity is what this whole phase was spent unpicking.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[Creator] ChannelRowClass unset -- %d row(s) resolved but none can be spawned."), Rows.Num());
		return;
	}

	for (const FAFLCreatorChannelRow& RowData : Rows)
	{
		if (UAFLW_CreatorChannelRowBase* RowWidget =
			CreateWidget<UAFLW_CreatorChannelRowBase>(this, ChannelRowClass))
		{
			RowWidget->SetRowData(RowData);
			ChannelRailContainer->AddChild(RowWidget);
		}
	}

	UE_LOG(LogAFLCombat, Log, TEXT("[Creator] rail rebuilt: %d row(s), schemaResolved=%d, focusAxis=%d"),
		Rows.Num(), IsSchemaResolved() ? 1 : 0, (int32)FocusAxis);
}

FAFLCreatorBuild UAFLW_Creator::AssembleBuild() const
{
	FAFLCreatorBuild Build;

	if (E_BuildName)
	{
		Build.DisplayName = E_BuildName->GetText().ToString();
	}

	for (const FAFLCreatorChannelRow& Row : Rows)
	{
		// UNSET STAYS UNSET. FAFLChannelValue defaults Resolved to WHITE, so writing every row would
		// turn "never touched" into "deliberately chose white". The rail's readout already distinguishes
		// them; the saved build must agree with what the player was shown.
		if (!Row.bHasValue)
		{
			continue;
		}
		// A row carries a resolved colour, never a cosmetic id -- so this is a CONTINUUM value.
		// MakeCatalog would set RequiresEntitlement() on a colour that has nothing to be entitled to.
		const FAFLChannelValue Value = FAFLChannelValue::MakeContinuum(Row.Colour);
		switch (Row.Channel)
		{
		case EAFLCreatorChannel::Body:  Build.BodyChannel  = Value; break;
		case EAFLCreatorChannel::Edge:  Build.EdgeChannel  = Value; break;
		case EAFLCreatorChannel::Glow:  Build.GlowChannel  = Value; break;
		case EAFLCreatorChannel::Visor: Build.VisorChannel = Value; break;
		default: break;
		}
	}
	return Build;
}

bool UAFLW_Creator::CommitBuild()
{
	APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAFLCosmeticLoadoutComponent* LC = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
	if (!LC)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_TEST[COMMIT] NOT DISPATCHED -- no loadout component on the player state."));
		return false;
	}

	const FAFLCreatorBuild Build = AssembleBuild();

	// LOG WHAT IS BEING SENT, not merely that something was. The four channel states are the whole
	// product, and "unset" is one of them.
	UE_LOG(LogAFLCombat, Display,
		TEXT("AFL_TEST[COMMIT] dispatch index=%d name='%s' body=%d edge=%d glow=%d visor=%d rows=%d"),
		EditingIndex, *Build.DisplayName,
		Build.BodyChannel.IsSet() ? 1 : 0, Build.EdgeChannel.IsSet() ? 1 : 0,
		Build.GlowChannel.IsSet() ? 1 : 0, Build.VisorChannel.IsSet() ? 1 : 0, Rows.Num());

	// DISPATCHING IS NOT SUCCEEDING. The server re-checks the cap, the name and the lapse state.
	LC->ServerSaveBuild(Build, EditingIndex);
	return true;
}

void UAFLW_Creator::HandleSaveClicked()
{
	CommitBuild();
}

void UAFLW_Creator::HandleCloseClicked()
{
	UE_LOG(LogAFLCombat, Log, TEXT("[Creator] close clicked -> deactivate."));
	DeactivateWidget();
}

void UAFLW_Creator::InitializeCreator(UAFLW_LoadoutBase* InLoadout)
{
	Loadout = InLoadout;
	RefreshFromSchema();
}

UAFLWalletComponent* UAFLW_Creator::ResolveWallet() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* PS = PC ? PC->PlayerState : nullptr;
	return PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
}

EAFLChannelAvailability UAFLW_Creator::StateFor(const EAFLCreatorChannel Channel) const
{
	switch (Channel)
	{
		case EAFLCreatorChannel::Body:  return Schema.BodyState;
		case EAFLCreatorChannel::Edge:  return Schema.EdgeState;
		case EAFLCreatorChannel::Glow:  return Schema.GlowState;
		case EAFLCreatorChannel::Visor: return Schema.VisorState;
	}
	return EAFLChannelAvailability::Absent;   // fails closed
}

FLinearColor UAFLW_Creator::ColourFor(const EAFLCreatorChannel Channel, bool& bOutHasValue) const
{
	bOutHasValue = false;
	const UAFLW_LoadoutBase* L = Loadout.Get();
	if (!L) { return FLinearColor::White; }

	const FAFLCosmeticSelection Sel = L->CreatorGetWorkingSelection();

	// PER-CHANNEL SETNESS IS NOT TRACKED, except for the visor. The selection carries one
	// bUseCreatorColors flag covering body/edge/glow together, plus a dedicated bVisorColorSet. So
	// "has a value" is answered honestly at the granularity the DATA actually has, rather than
	// inventing a per-channel flag the struct does not carry. Named here because a reader will
	// otherwise assume the finer granularity exists.
	switch (Channel)
	{
		case EAFLCreatorChannel::Body:
			bOutHasValue = Sel.bUseCreatorColors != 0;
			return Sel.CreatorBodyColor;
		case EAFLCreatorChannel::Edge:
			bOutHasValue = Sel.bUseCreatorColors != 0;
			return Sel.CreatorEdgeColor;
		case EAFLCreatorChannel::Glow:
			bOutHasValue = Sel.bUseCreatorColors != 0;
			return Sel.CreatorGlowColor;
		case EAFLCreatorChannel::Visor:
			bOutHasValue = Sel.bVisorColorSet != 0;   // an EXPLICIT choice, distinct from the body mirror
			return Sel.CreatorVisorColor;
	}
	return FLinearColor::White;
}

FText UAFLW_Creator::ReasonFor(const EAFLCreatorChannel Channel, const EAFLChannelAvailability State) const
{
	if (State == EAFLChannelAvailability::Connected) { return FText::GetEmpty(); }

	// THE MASTER IS THE REASON. The schema carries ResolvedFromMaster precisely so the UI can say WHY
	// rather than just refusing. A disabled control with no explanation is indistinguishable from a
	// bug -- to the player and to the next developer.
	const FText Master = FText::FromName(Schema.ResolvedFromMaster);
	const FText Label  = ChannelLabel(Channel);

	if (State == EAFLChannelAvailability::PresentButInert)
	{
		// Restorable by a material change -- deliberately worded as "not available on this chassis"
		// rather than "missing", because the parameter IS there and a future material pass can wake it.
		return FText::Format(
			NSLOCTEXT("AFLCreator", "ReasonInert", "{0} colour isn't available on this chassis ({1})."),
			Label, Master);
	}
	return FText::Format(
		NSLOCTEXT("AFLCreator", "ReasonAbsent", "This chassis has no {0} to tint ({1})."),
		Label, Master);
}

FText UAFLW_Creator::BuildReadout(const FLinearColor& C, const bool bHasValue)
{
	// NO FABRICATED HEX. An unset channel reads as a dash; showing #FF0000 for "nothing chosen" would
	// claim the player picked red.
	if (!bHasValue) { return NSLOCTEXT("AFLCreator", "ReadoutUnset", "—"); }

	const FLinearColor HSV = C.LinearRGBToHSV();
	const FColor       SRGB = C.ToFColor(/*bSRGB=*/true);

	// BOTH forms, settled by the wireframe state sheet: hex is recognisable and copy-pasteable; HSV
	// names the axis the arc actually moves and makes the clamp legible -- a player can SEE that
	// saturation is sitting on its 0.55 floor instead of wondering why it will not go paler.
	return FText::FromString(FString::Printf(
		TEXT("#%02X%02X%02X  ·  H %d° S %.2f V %.2f"),
		SRGB.R, SRGB.G, SRGB.B,
		FMath::RoundToInt(HSV.R), HSV.G, HSV.B));
}

void UAFLW_Creator::RefreshFromSchema()
{
	Rows.Reset();
	bSchemaResolved = false;

	UAFLW_LoadoutBase* L = Loadout.Get();
	if (!L)
	{
		OnChannelRowsChanged();
		return;
	}

	Schema = L->CreatorGetSchema();

	// FAILS CLOSED. ResolvedFromMaster == None means the schema has not resolved, so we claim NO
	// channels rather than guessing a set. A creator that invents a rail while loading teaches the
	// player that channels appear and disappear.
	if (Schema.ResolvedFromMaster.IsNone())
	{
		OnChannelRowsChanged();
		return;
	}
	bSchemaResolved = true;
	RebuildRows();
}

#if !UE_BUILD_SHIPPING
void UAFLW_Creator::DebugBuildRowsFromSchema(const FAFLCreatorChannelSchema& InSchema)
{
	Schema = InSchema;
	bSchemaResolved = !InSchema.ResolvedFromMaster.IsNone();
	Rows.Reset();
	if (bSchemaResolved) { RebuildRows(); }
	else { OnChannelRowsChanged(); }
}
#endif

void UAFLW_Creator::RebuildRows()
{
	Rows.Reset();
	UAFLW_LoadoutBase* L = Loadout.Get();

	for (const EAFLCreatorChannel Ch : GRailOrder)
	{
		const EAFLChannelAvailability State = StateFor(Ch);

		FAFLCreatorChannelRow Row;
		Row.Channel       = Ch;
		Row.Label         = ChannelLabel(Ch);
		Row.State         = State;
		Row.bInteractive  = (State == EAFLChannelAvailability::Connected);
		Row.Reason        = ReasonFor(Ch, State);
		Row.Colour        = ColourFor(Ch, Row.bHasValue);
		Row.HueDegrees    = Row.bHasValue ? AFLCreatorGamut::HueOf(Row.Colour) : 0.0f;
		Row.Readout       = BuildReadout(Row.Colour, Row.bHasValue);
		Row.bLinked       = L ? L->CreatorLinks.IsLinked(Ch) : false;

		// Connected on an unaudited master is "present, inertness unknown" -- NOT "measured to
		// render". The caveat rides on the row so the UI cannot quietly upgrade it to a claim.
		Row.bUnauditedCaveat = (State == EAFLChannelAvailability::Connected) && !Schema.bMasterAudited;

		// EVERY channel is emitted, including Absent ones. Hiding a channel makes a missing feature
		// look like a bug; the WBP renders disabled rows with their reason and tells the two disabled
		// states apart by fill, icon and text -- never by hue.
		Rows.Add(MoveTemp(Row));
	}

	OnChannelRowsChanged();
}

FLinearColor UAFLW_Creator::GetArcTrackColour(const float HueDegrees) const
{
	// The TRACK is the gamut. Every point the player can reach is generated by the same function the
	// server commits with, so there is nowhere out-of-gamut to drag to and no snap-back to feel.
	return AFLCreatorGamut::FromHue(HueDegrees);
}

void UAFLW_Creator::SetChannelHue(const EAFLCreatorChannel Channel, const float HueDegrees)
{
	UAFLW_LoadoutBase* L = Loadout.Get();
	if (!L) { return; }

	// REFUSE ON STATE, not on widget enable-ness. A disabled row must not be able to write even if a
	// stray input path reaches it -- the schema is the authority, not the visual state.
	if (StateFor(Channel) != EAFLChannelAvailability::Connected) { return; }

	bool bHadValue = false;
	const FLinearColor Current = ColourFor(Channel, bHadValue);

	// ACHROMATIC START. Dragging hue from grey/black/white is a real path: WithHue lifts saturation to
	// the floor at the chosen hue, which is a visible, expected transition rather than a glitch. From
	// an unset channel we take FromHue so we do not inherit a neutral's degenerate saturation.
	const FLinearColor Next = bHadValue
		? AFLCreatorGamut::WithHue(Current, HueDegrees)
		: AFLCreatorGamut::FromHue(HueDegrees);

	// ONE CLAMP: CreatorSetChannel clamps on entry with the shared gamut. We do NOT pre-clamp here --
	// a second clamp is a second implementation of one rule, and two drift.
	L->CreatorSetChannel(Channel, Next);

	// Linked channels move together. Reading the link state from the shipped struct rather than
	// keeping a UI-side copy, so the toggle and the behaviour cannot disagree.
	for (const EAFLCreatorChannel Other : GRailOrder)
	{
		if (Other == Channel) { continue; }
		if (!L->CreatorLinks.IsLinked(Other) || !L->CreatorLinks.IsLinked(Channel)) { continue; }
		if (StateFor(Other) != EAFLChannelAvailability::Connected) { continue; }

		bool bOtherHad = false;
		const FLinearColor OtherCur = ColourFor(Other, bOtherHad);
		L->CreatorSetChannel(Other, bOtherHad
			? AFLCreatorGamut::WithHue(OtherCur, HueDegrees)
			: AFLCreatorGamut::FromHue(HueDegrees));
	}

	// THE PREVIEW IS THE PRODUCT: push through the shipping resolve path, the same one the gameplay
	// pawn uses on possession. A second render route would reintroduce the divergence that rule bans.
	L->CreatorApplyPreview();

	RefreshFromSchema();
}

void UAFLW_Creator::SetChannelLinked(const EAFLCreatorChannel Channel, const bool bLinked)
{
	if (UAFLW_LoadoutBase* L = Loadout.Get())
	{
		// Links default OFF and stay off until a pairing is ruled. The toggle exists; the UI must not
		// present any pairing as a default.
		L->CreatorLinks.SetLinked(Channel, bLinked);
		RefreshFromSchema();
	}
}

bool UAFLW_Creator::IsChannelLinked(const EAFLCreatorChannel Channel) const
{
	const UAFLW_LoadoutBase* L = Loadout.Get();
	return L && L->CreatorLinks.IsLinked(Channel);
}

void UAFLW_Creator::RotatePreview(const float DeltaYawDegrees)
{
	if (UAFLW_LoadoutBase* L = Loadout.Get())
	{
		L->CreatorRotatePreview(DeltaYawDegrees);
	}
}

int32 UAFLW_Creator::GetSlotsUsed() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* PS = PC ? PC->PlayerState : nullptr;
	const UAFLCosmeticLoadoutComponent* LC = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
	// UNLOCKED, not total. A locked build is kept and rendering but sits OUTSIDE the cap, so counting it
	// made this read "5 / 2" against the very cap it is displayed beside.
	return LC ? LC->CountUnlockedBuilds() : 0;
}

int32 UAFLW_Creator::GetSlotCap() const
{
	// DELEGATES TO THE SERVER'S LADDER. This computed the cap itself, so the number shown to the player
	// and the number the server would enforce were two independent calculations free to disagree. The
	// component owns it now; this reads the same answer the gate uses.
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const APlayerState* PS = PC->PlayerState)
		{
			if (const UAFLCosmeticLoadoutComponent* LC = PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>())
			{
				return LC->GetEffectiveSlotCap();
			}
		}
	}
	// Nothing to ask -- show the baseline rather than a number invented here.
	return UAFLCosmeticLoadoutComponent::SlotBaseline;
}

FText UAFLW_Creator::GetSlotCounterText() const
{
	return FText::FromString(FString::Printf(TEXT("%d / %d"), GetSlotsUsed(), GetSlotCap()));
}

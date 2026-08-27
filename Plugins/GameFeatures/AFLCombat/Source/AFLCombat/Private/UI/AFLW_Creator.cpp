// CC-5.2 -- the creator widget's behaviour layer. See AFLW_Creator.h for why the rail is data.
#include "UI/AFLW_Creator.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Cosmetics/AFLCharacterPartMap.h"   // ResolveCharacterPart -- the line-availability query

#include "UI/AFLW_LoadoutBase.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "AFLCombat.h"                  // LogAFLCombat -- this file never logged before
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/CheckBox.h"     // the link toggle
#include "Components/Image.h"        // the channel swatch        // the slot readout   // the build name the player types
#include "Components/PanelWidget.h"       // the rail the rows are spawned into           // CC-5: the creator's own way out
#include "Engine/TextureRenderTarget2D.h"   // region B: the preview capture the creator displays
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
	// CORRECTED 2026-08-27: EXISTENCE IS NOT FOCUSABILITY.
	//
	// The list used to end in "Root_Shell", and the loop accepted any widget it FOUND. Root_Shell
	// exists -- it is the root CanvasPanel -- so the lookup succeeded, handed CommonUI a widget that
	// can never take focus, and the log read "Root_Shell in WBP_AFL_Creator does not support focus".
	// The fallback that was meant to guarantee focus is what prevented it: a panel always resolves, so
	// the genuinely focusable candidates after it were never reached.
	//
	// Now the candidate must SUPPORT focus, not merely exist, and the panels are gone from the list --
	// a CanvasPanel is never a focus target, so naming one is always a bug.
	static const TCHAR* const Preferred[] = {
		TEXT("C_ChannelRail"),   // the rail, if a focusable container is ever authored
		TEXT("ChannelRail"),
		TEXT("E_BuildName"),     // the name field -- the first thing a new build wants
		TEXT("F_Save"),
		TEXT("CloseButton"),
	};
	for (const TCHAR* Name : Preferred)
	{
		UWidget* W = GetWidgetFromName(FName(Name));
		if (!W)
		{
			continue;
		}

		// ASK SLATE, NOT UMG. UWidget publishes no IsFocusable in 5.6 -- focusability is a property of
		// the underlying SWidget, so SupportsKeyboardFocus() is the only honest test. Belt and braces:
		// a UPanelWidget is excluded structurally, because a container is never a focus target and
		// naming one is always an authoring mistake, cached widget or not.
		const bool bIsPanel = W->IsA<UPanelWidget>();
		const TSharedPtr<SWidget> Slate = W->GetCachedWidget();
		const bool bCanFocus = !bIsPanel && Slate.IsValid() && Slate->SupportsKeyboardFocus();

		if (bCanFocus)
		{
			return W;
		}
		{
			// Named, not skipped in silence: a candidate that exists but cannot take focus is a
			// mis-authored preference, and it is invisible unless it says so.
			UE_LOG(LogAFLCombat, Verbose,
				TEXT("[Creator] focus candidate '%s' exists but does not support focus -- skipping."),
				Name);
		}
	}
	UE_LOG(LogAFLCombat, Verbose,
		TEXT("[Creator] no focusable preferred region present -- focusing the creator itself."));
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

	// REGIONS A and F, wired where the existing buttons are wired -- one place, so a future reader
	// finds every binding together rather than discovering region A was hooked up somewhere else.
	if (F_Revert)
	{
		F_Revert->OnClicked.RemoveDynamic(this, &UAFLW_Creator::HandleRevertClicked);
		F_Revert->OnClicked.AddDynamic(this, &UAFLW_Creator::HandleRevertClicked);
	}
	if (A_ChassisManny)
	{
		A_ChassisManny->OnClicked.RemoveDynamic(this, &UAFLW_Creator::HandleChassisMannyClicked);
		A_ChassisManny->OnClicked.AddDynamic(this, &UAFLW_Creator::HandleChassisMannyClicked);
	}
	if (A_ChassisProMod)
	{
		A_ChassisProMod->OnClicked.RemoveDynamic(this, &UAFLW_Creator::HandleChassisProModClicked);
		A_ChassisProMod->OnClicked.AddDynamic(this, &UAFLW_Creator::HandleChassisProModClicked);
	}
	else
	{
		// SAYS SO. An unbound commit button is exactly the "built, correct, unreachable" shape this
		// step exists to end.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[Creator] F_Save not bound -- the creator has no way to commit a build."));
	}
}

void UAFLW_CreatorChannelRowBase::SetRowData(const FAFLCreatorChannelRow& InRow)
{
	Row = InRow;

	if (Row_Label)
	{
		Row_Label->SetText(Row.Label);
	}
	if (Row_Readout)
	{
		Row_Readout->SetText(Row.Readout);
	}
	if (Row_Reason)
	{
		// A REASON IS SHOWN ONLY WHEN THERE IS ONE. An empty reason line under every row would teach the
		// player to stop reading the line that matters when a channel is genuinely unavailable.
		Row_Reason->SetText(Row.Reason);
		Row_Reason->SetVisibility(Row.Reason.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (Row_StateBadge)
	{
		// COLLAPSED WHEN CONNECTED, and never showing an enum name.
		//
		// This printed GetDisplayNameTextByValue straight onto the screen, so every working row read
		// "Connected" -- engineering vocabulary as the most prominent text on a colour editor, and on
		// other rows it would have read "PresentButInert" or "Absent". A working control does not
		// announce that it works. Non-connected rows explain themselves through Row_Reason, which
		// already exists and already collapses when empty; the badge was competing with it.
		const bool bShowBadge = (Row.State != EAFLChannelAvailability::Connected);
		Row_StateBadge->SetVisibility(bShowBadge
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Row_StateBadge->SetText(FText::GetEmpty());
	}
	if (Row_Swatch)
	{
		// UNSET IS SHOWN AS UNSET. Painting FLinearColor::White on a channel the player has not touched
		// would claim they chose white, which is a colour they can actually choose.
		// Inherited colours SHOW. The swatch answers "what colour is this channel", and the robot is
		// wearing one whether or not the player picked it. The unset-is-unset rule above still holds
		// where nothing is worn either -- bInherited is false then and the swatch stays hidden.
		Row_Swatch->SetColorAndOpacity(Row.Colour);
		Row_Swatch->SetVisibility((Row.bHasValue || Row.bInherited)
			? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	if (Row_LinkToggle)
	{
		Row_LinkToggle->SetIsChecked(Row.bLinked);
		Row_LinkToggle->SetIsEnabled(Row.bInteractive);
	}

	// The whole row reads as unavailable when it is, rather than looking live and doing nothing.
	SetIsEnabled(Row.bInteractive);

	OnRowDataSet();
}

void UAFLW_Creator::NativeOnActivated()
{
	Super::NativeOnActivated();

	// ON ACTIVATE, NOT ON CONSTRUCT. The schema resolves against the loadout's DisplayPawn, which may not
	// have spawned when this widget is constructed -- resolving early is how an earlier probe ended up
	// reporting channels against whatever material happened to be reachable.
	RefreshFromSchema();
	RebuildChannelRows();

	// Regions A and B paint with the rail, not after it. Region A DETERMINES the rail contents
	// (CC_UI_HANDOFF 2), so a picker painted later would describe a chassis the rail has moved past.
	RefreshChassisPicker();
	RefreshPreviewViewport();

	// Paint once now (a set that replicated before this screen opened produces no edge to listen for),
	// then follow the authoritative set from here on.
	RefreshSlotCounter();
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APlayerState* PS = PC->PlayerState)
		{
			if (UAFLCosmeticLoadoutComponent* LC = PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>())
			{
				LC->OnBuildSetChanged.RemoveAll(this);
				LC->OnBuildSetChanged.AddUObject(this, &UAFLW_Creator::RefreshSlotCounter);
			}
		}
	}
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

void UAFLW_Creator::RefreshSlotCounter()
{
	if (!D_SlotCounter)
	{
		return;
	}
	// UNLOCKED / CAP. A locked build sits outside the cap, so showing the total would print "5 / 2".
	D_SlotCounter->SetText(GetSlotCounterText());
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

bool UAFLW_Creator::LoadBuild(int32 Index)
{
	UAFLW_LoadoutBase* L = Loadout.Get();
	APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAFLCosmeticLoadoutComponent* LC = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
	if (!L || !LC)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TEST[LOADBUILD] REFUSED -- loadout=%d component=%d."),
			L ? 1 : 0, LC ? 1 : 0);
		return false;
	}

	const FAFLCreatorBuildSet& Set = LC->GetBuildSet();
	if (!Set.Builds.IsValidIndex(Index))
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_TEST[LOADBUILD] REFUSED -- index %d names no build (held=%d)."),
			Index, Set.Builds.Num());
		return false;
	}
	const FAFLCreatorBuild& Build = Set.Builds[Index];

	// THROUGH THE CHANNEL SETTER, NOT AROUND IT. The rail reads colour from the working selection and
	// the preview renders from the same one; writing Rows directly would show a colour the preview does
	// not have.
	//
	// AN UNSET CHANNEL IS NOT PUSHED. Resolved defaults to WHITE, so writing all four would turn "never
	// set Glow" into "chose white".
	int32 Pushed = 0;
	auto Push = [&](EAFLCreatorChannel Ch, const FAFLChannelValue& V)
	{
		if (V.IsSet()) { L->CreatorSetChannel(Ch, V.Resolved); ++Pushed; }
	};
	Push(EAFLCreatorChannel::Body,  Build.BodyChannel);
	Push(EAFLCreatorChannel::Edge,  Build.EdgeChannel);
	Push(EAFLCreatorChannel::Glow,  Build.GlowChannel);
	Push(EAFLCreatorChannel::Visor, Build.VisorChannel);

	EditingIndex = Index;
	if (E_BuildName)
	{
		E_BuildName->SetText(FText::FromString(Build.DisplayName));
	}

	// A read-only build is still LOADED and still RENDERS -- the lapse rule's whole promise is that a
	// player keeps looking like the robot they built. It simply cannot be saved over.
	SetIsEnabled(!Build.bReadOnly);

	RefreshFromSchema();
	RebuildChannelRows();
	RefreshSlotCounter();

	UE_LOG(LogAFLCombat, Display,
		TEXT("AFL_TEST[LOADBUILD] index=%d name='%s' readOnly=%d channelsPushed=%d rows=%d"),
		Index, *Build.DisplayName, Build.bReadOnly ? 1 : 0, Pushed, Rows.Num());

	// STATED, NOT HIDDEN: body/edge/glow share ONE bUseCreatorColors flag in the selection, so a build
	// with only some of the three set cannot round-trip exactly -- setting any one makes all three read
	// as set. A property of the selection struct, not of this loader.
	const int32 ColourChannels = (Build.BodyChannel.IsSet() ? 1 : 0)
		+ (Build.EdgeChannel.IsSet() ? 1 : 0) + (Build.GlowChannel.IsSet() ? 1 : 0);
	if (ColourChannels > 0 && ColourChannels < 3)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_TEST[LOADBUILD] %d of 3 colour channels set -- the selection carries ONE flag for "
				"body/edge/glow, so all three will read as set."), ColourChannels);
	}
	return true;
}

void UAFLW_Creator::BeginNewBuild()
{
	// INDEX_NONE IS THE APPEND CASE and the only one the slot cap gates. Editing an existing build is
	// always allowed; creating another may save locked.
	EditingIndex = INDEX_NONE;
	if (E_BuildName)
	{
		E_BuildName->SetText(FText::GetEmpty());
	}
	SetIsEnabled(true);
	RefreshFromSchema();
	RebuildChannelRows();
	RefreshSlotCounter();
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_TEST[LOADBUILD] NEW build (EditingIndex=INDEX_NONE) rows=%d"), Rows.Num());
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

namespace
{
	/**
	 * The colour the chassis is ALREADY WEARING, read from the material that renders it.
	 *
	 * The worn colour is not in FAFLCosmeticSelection -- it is baked into the identity's material
	 * instance -- so a new build legitimately has no selection value and the rail showed four labels,
	 * no swatches and "--" on a COLOUR EDITOR. The value IS reachable: the same slot-0 / slot-1
	 * materials the schema derives from carry these parameters, which is how SchemaProbe read
	 * TeamColor off M_AFL_Character.
	 *
	 * Same slot split as the schema, for the same reason: a channel's value must come from the
	 * material that renders it, or the rail shows a colour the robot is not wearing.
	 */
	bool AFLReadWornChannel(const APawn* Pawn, EAFLCreatorChannel Channel, FLinearColor& Out)
	{
		if (!Pawn) { return false; }

		const int32 Slot = (Channel == EAFLCreatorChannel::Visor) ? 1 : 0;
		const TCHAR* Param =
			(Channel == EAFLCreatorChannel::Body)  ? TEXT("TeamColor")     :
			(Channel == EAFLCreatorChannel::Edge)  ? TEXT("EdgeGlowColor") :
			(Channel == EAFLCreatorChannel::Glow)  ? TEXT("EmissiveColor") :
			                                         TEXT("BaseTint");

		TArray<UChildActorComponent*> CACs;
		const_cast<APawn*>(Pawn)->GetComponents<UChildActorComponent>(CACs);
		for (const UChildActorComponent* CAC : CACs)
		{
			AActor* Child = CAC ? CAC->GetChildActor() : nullptr;
			if (!Child) { continue; }
			TArray<UMeshComponent*> Meshes;
			Child->GetComponents<UMeshComponent>(Meshes);
			for (UMeshComponent* Mesh : Meshes)
			{
				if (!Mesh || Mesh->GetNumMaterials() <= Slot) { continue; }
				if (UMaterialInterface* Mat = Mesh->GetMaterial(Slot))
				{
					FLinearColor V(ForceInit);
					if (Mat->GetVectorParameterValue(FMaterialParameterInfo(FName(Param)), V))
					{
						Out = V;
						return true;
					}
				}
			}
		}
		return false;
	}
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
	// NOTHING IS DISCARDED UNTIL THERE IS SOMETHING BETTER TO PUT IN ITS PLACE.
	//
	// This used to Reset() the rows and clear bSchemaResolved on entry. That made a FAILED re-resolve
	// destructive: InitializeCreator resolved the schema during the push, then a second call from
	// NativeOnActivated re-resolved into an empty result and wiped it -- measured as
	// "SCHEMA ... available=4" at :820 followed by "0 row(s), schemaResolved=0" at :822.
	//
	// Both silent paths that produce that empty result -- a null loadout component, and a null display
	// pawn -- return a default-constructed schema without logging, so the clobber left no trace and
	// surfaced only as an empty rail.
	UAFLW_LoadoutBase* L = Loadout.Get();
	if (!L)
	{
		if (!bSchemaResolved) { Rows.Reset(); OnChannelRowsChanged(); }
		return;
	}

	const FAFLCreatorChannelSchema Candidate = L->CreatorGetSchema();

	// FAILS CLOSED, BUT NOT BACKWARDS. ResolvedFromMaster == None means THIS attempt did not resolve.
	// If an earlier one did, keep it: a creator that claims no channels because a later lookup was
	// unlucky teaches the player that channels appear and disappear, which is the very thing the
	// original fail-closed rule was written to prevent.
	if (Candidate.ResolvedFromMaster.IsNone())
	{
		if (bSchemaResolved)
		{
			UE_LOG(LogAFLCombat, Verbose,
				TEXT("[Creator] re-resolve produced nothing; keeping the schema from %s."),
				*Schema.ResolvedFromMaster.ToString());
			return;
		}
		Rows.Reset();
		OnChannelRowsChanged();
		return;
	}

	Schema = Candidate;
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

		// INHERITED, NOT CHOSEN. With nothing picked, fall back to what the robot is actually wearing
		// so the rail reads "your robot's colour, change it" rather than four blanks on a colour
		// editor. bHasValue stays FALSE: CommitRowsToBuild keys off it, and an inherited colour is not
		// a choice -- seeding it would turn "never touched" into "deliberately chose this" on save.
		Row.bInherited = false;
		if (!Row.bHasValue)
		{
			FLinearColor Worn;
			if (AFLReadWornChannel(L ? L->GetPreviewPawn() : nullptr, Ch, Worn))
			{
				Row.Colour     = Worn;
				Row.bInherited = true;
			}
		}

		const bool bShowValue = Row.bHasValue || Row.bInherited;
		Row.HueDegrees    = bShowValue ? AFLCreatorGamut::HueOf(Row.Colour) : 0.0f;
		Row.Readout       = BuildReadout(Row.Colour, bShowValue);
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


// ===== CHASSIS (C2) =================================================================================

namespace
{
	/** The Pro Mod blank base wears this mesh. The chassis IS the body base, so the mesh is the read. */
	const TCHAR* GAFLProModMesh = TEXT("SKM_IRONICS_Blank");

	/** Which chassis a pawn is WEARING, from the skeletal mesh its part actors carry.
	 *
	 *  Read from the MESH rather than from the identity id, because the id proved to be the wrong
	 *  instrument twice: _X suffixes are character-era legacy, and a MID name (MID_MI_IRONICS_Body_Red_0)
	 *  tells you nothing reliable about the base. The mesh is what the ruling defines the chassis by. */
	EAFLChassisLine AFLChassisOfPawn(const APawn* Pawn)
	{
		if (!Pawn) { return EAFLChassisLine::Manny; }

		TArray<UChildActorComponent*> CACs;
		const_cast<APawn*>(Pawn)->GetComponents<UChildActorComponent>(CACs);
		for (const UChildActorComponent* CAC : CACs)
		{
			AActor* Child = CAC ? CAC->GetChildActor() : nullptr;
			if (!Child) { continue; }
			TArray<USkeletalMeshComponent*> Meshes;
			Child->GetComponents<USkeletalMeshComponent>(Meshes);
			for (const USkeletalMeshComponent* SK : Meshes)
			{
				const USkeletalMesh* M = SK ? SK->GetSkeletalMeshAsset() : nullptr;
				if (M && M->GetName().Contains(GAFLProModMesh))
				{
					return EAFLChassisLine::ProMod;
				}
			}
		}
		return EAFLChassisLine::Manny;
	}
}

EAFLChassisLine UAFLW_Creator::GetCurrentChassisLine() const
{
	UAFLW_LoadoutBase* L = const_cast<UAFLW_LoadoutBase*>(Loadout.Get());
	return AFLChassisOfPawn(L ? L->GetPreviewPawn() : nullptr);
}

bool UAFLW_Creator::IsChassisLineAvailable(const EAFLChassisLine Line, FText& OutReason) const
{
	OutReason = FText::GetEmpty();

	if (Line == GetCurrentChassisLine())
	{
		return true;   // already worn; the card renders as the current selection
	}

	UAFLW_LoadoutBase* L = const_cast<UAFLW_LoadoutBase*>(Loadout.Get());
	if (!L)
	{
		OutReason = NSLOCTEXT("AFLCreator", "ChassisNoLoadout", "Not available right now.");
		return false;
	}
	const UAFLCharacterPartMap* Map = L->GetDisplayPartMap();
	if (!Map)
	{
		OutReason = NSLOCTEXT("AFLCreator", "ChassisNoMap", "Chassis data is not loaded.");
		return false;
	}

	// PRO MOD -- one shared blank base, addressed by whichever identity key the part map gives it.
	if (Line == EAFLChassisLine::ProMod)
	{
		if (ProModChassisIdentityId.IsNone())
		{
			OutReason = NSLOCTEXT("AFLCreator", "ChassisProModUnmapped",
				"The Pro Mod chassis isn't available yet.");
			UE_LOG(LogAFLCombat, Verbose,
				TEXT("[Creator] Pro Mod unavailable: ProModChassisIdentityId unset. "
				     "B_AFL_Robot_Chassis_X is referenced by the part map but has no identity key."));
			return false;
		}
		if (Map->ResolveCharacterPart(ProModChassisIdentityId).IsNull())
		{
			OutReason = NSLOCTEXT("AFLCreator", "ChassisProModNoBody",
				"The Pro Mod chassis isn't available yet.");
			UE_LOG(LogAFLCombat, Verbose,
				TEXT("[Creator] Pro Mod unavailable: '%s' is not in the part map."),
				*ProModChassisIdentityId.ToString());
			return false;
		}
		return true;
	}

	// MANNY -- per-identity bodies. Reachable when the working identity maps to one.
	const FName Id = L->CreatorGetWorkingSelection().CharacterId;
	if (Map->ResolveCharacterPart(Id).IsNull())
	{
		OutReason = NSLOCTEXT("AFLCreator", "ChassisMannyNoBody",
			"This chassis has no body for your robot yet.");
		UE_LOG(LogAFLCombat, Verbose,
			TEXT("[Creator] Manny unavailable: '%s' is not in the part map."), *Id.ToString());
		return false;
	}
	return true;
}

bool UAFLW_Creator::SelectChassisLine(const EAFLChassisLine Line)
{
	FText Reason;
	if (!IsChassisLineAvailable(Line, Reason))
	{
		// Refuses rather than half-applying. Equipping an id the map cannot resolve would leave the
		// selection claiming a chassis the pawn is not wearing, and the rail would then derive its
		// channels from the OLD body while the card showed the new one -- which looks like it worked.
		UE_LOG(LogAFLCombat, Warning, TEXT("[Creator] SelectChassisLine REFUSED: %s"), *Reason.ToString());
		return false;
	}
	if (Line == GetCurrentChassisLine())
	{
		return true;
	}

	UAFLW_LoadoutBase* L = Loadout.Get();
	if (!L) { return false; }

	const FName Target = (Line == EAFLChassisLine::ProMod)
		? ProModChassisIdentityId
		: L->CreatorGetWorkingSelection().CharacterId;

	L->EquipForAxis(EAFLLoadoutAxis::Identity, Target);

	// The channel count is a property of the chassis -- Manny offers three body channels, Pro Mod two
	// (TeamColor is inert on M_AFL_Character). So the rail MUST be re-derived here; this is the
	// "call on chassis change" the RebuildRows contract already asks for.
	RefreshFromSchema();
	RebuildChannelRows();

	UE_LOG(LogAFLCombat, Display, TEXT("[Creator] chassis -> %s (identity %s); rail re-derived"),
		Line == EAFLChassisLine::ProMod ? TEXT("ProMod") : TEXT("Manny"), *Target.ToString());
	return true;
}


// ===== REGION A -- CHASSIS PICKER ===================================================================

void UAFLW_Creator::RefreshChassisPicker()
{
	if (!A_ChassisPicker && !A_ChassisManny && !A_ChassisProMod)
	{
		// Reported once, not per tile: an unauthored region A is a WBP gap, not a runtime fault, and
		// the screen still works without it.
		UE_LOG(LogAFLCombat, Verbose,
			TEXT("[Creator] region A not authored -- no chassis picker bound."));
		return;
	}

	const EAFLChassisLine Current = GetCurrentChassisLine();

	FText MannyReason, ProModReason;
	const bool bMannyOk  = IsChassisLineAvailable(EAFLChassisLine::Manny,  MannyReason);
	const bool bProModOk = IsChassisLineAvailable(EAFLChassisLine::ProMod, ProModReason);

	// LABELS ARE ALL-CAPS DISPLAY TEXT. Section 3 reserves Orbitron for identity-carrying text, and a
	// chassis name is exactly that -- it names the product line, not a control.
	if (A_ChassisMannyLabel)
	{
		A_ChassisMannyLabel->SetText(NSLOCTEXT("AFLCreator", "ChassisManny", "MANNY"));
	}
	if (A_ChassisProModLabel)
	{
		A_ChassisProModLabel->SetText(NSLOCTEXT("AFLCreator", "ChassisProMod", "PRO MOD"));
	}

	// A tile is interactive only if switching to it would actually WORK. Enabling on anything weaker
	// produces a tile that accepts a click and does nothing -- the states table forbids it, and this
	// is the same rule the claim button follows.
	// PRO MOD IS THE DEFAULT LINE; MANNY IS PRESENT BUT GATED ON ITS OWN AVAILABILITY.
	//
	// Shown-and-disabled, never hidden: CC_UI_HANDOFF 5.1 treats an unavailable option as dimmed WITH
	// A REASON, because a line that vanishes reads as a missing feature while a dimmed one reads as
	// not-yet. Same treatment the PresentButInert channel row gets.
	if (A_ChassisManny)
	{
		A_ChassisManny->SetIsEnabled(bMannyOk && Current != EAFLChassisLine::Manny);
	}
	if (A_ChassisProMod)
	{
		A_ChassisProMod->SetIsEnabled(bProModOk && Current != EAFLChassisLine::ProMod);
	}
	if (A_ChassisMannyReason)
	{
		const bool bShow = !bMannyOk && Current != EAFLChassisLine::Manny;
		A_ChassisMannyReason->SetText(bShow ? MannyReason : FText::GetEmpty());
		A_ChassisMannyReason->SetVisibility(bShow
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// THE REASON TRAVELS WITH THE REFUSAL. A dimmed tile with no explanation reads as broken; with a
	// reason it reads as not-yet-available, which is what it is.
	if (A_ChassisProModReason)
	{
		const bool bShow = !bProModOk && Current != EAFLChassisLine::ProMod;
		A_ChassisProModReason->SetText(bShow ? ProModReason : FText::GetEmpty());
		A_ChassisProModReason->SetVisibility(bShow
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UAFLW_Creator::HandleChassisMannyClicked()
{
	if (SelectChassisLine(EAFLChassisLine::Manny))
	{
		// The rail length is a property of the chassis, so both regions refresh together. Refreshing
		// only the picker would leave region C describing the chassis the player just left.
		RefreshChassisPicker();
		RefreshPreviewViewport();
	}
}

void UAFLW_Creator::HandleChassisProModClicked()
{
	if (SelectChassisLine(EAFLChassisLine::ProMod))
	{
		RefreshChassisPicker();
		RefreshPreviewViewport();
	}
}

// ===== REGION B -- PREVIEW VIEWPORT =================================================================

void UAFLW_Creator::RefreshPreviewViewport()
{
	if (!B_PreviewImage)
	{
		UE_LOG(LogAFLCombat, Verbose, TEXT("[Creator] region B not authored -- no preview image bound."));
		return;
	}

	UAFLW_LoadoutBase* L = Loadout.Get();
	if (!L)
	{
		return;
	}

	// THE EXISTING CAPTURE, not a second one. CREATOR_SSOT 5.3: the preview IS the product, and a
	// separate capture would drift from the loadout's in lighting or pose -- which reads as
	// bait-and-switch on the first match load.
	UTextureRenderTarget2D* RT = L->GetPreviewRenderTarget();
	if (!RT)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[Creator] region B bound but the loadout has no PreviewRT -- the viewport would "
			     "render nothing. SetupPreviewCapture has not run."));
		return;
	}

	// SetBrushResourceObject, not SetBrushFromTexture: the latter takes a UTexture2D and a render
	// target is not one. The brush accepts any UObject resource, which is how the loadout already
	// displays this same capture.
	B_PreviewImage->SetBrushResourceObject(RT);
}

// ===== REGION F -- REVERT ===========================================================================

void UAFLW_Creator::HandleRevertClicked()
{
	// CREATOR_SSOT 5.3: every choice reversible without loss. Revert existed in the WBP and was never
	// bound, so the promise had no code behind it.
	UAFLW_LoadoutBase* L = Loadout.Get();
	if (!L)
	{
		return;
	}

	// DISCARD, THEN RE-RUN THE NORMAL APPLY -- never restore from a cached copy.
	//
	// CreatorRevertWorking clears the working selection AND its seeded flag, so the next read
	// re-seeds from the authoritative component rather than from anything this widget held. Then
	// CreatorApplyPreview pushes that through the SHIPPING resolve path (SetPreviewSelection ->
	// GetEffectiveSelection -> BuildColorOverride), which is the same route the gameplay pawn takes
	// on possession.
	//
	// Without the apply the pawn keeps showing the reverted-away colours while the rail reads
	// correct -- the screen would say one thing and the robot another, which is the exact
	// bait-and-switch CREATOR_SSOT 5.3 forbids.
	L->CreatorRevertWorking();
	L->CreatorApplyPreview();

	RebuildRows();
	RefreshChassisPicker();
	RefreshPreviewViewport();
}

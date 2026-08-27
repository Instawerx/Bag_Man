// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLStyleRepointLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Components/BorderSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#endif

TArray<FString> UAFLStyleRepointLibrary::RepointTextStyle(
	const FString& BlueprintPath,
	const FString& FromStyleClassName,
	const FString& ToStyleObjectPath,
	bool bApply)
{
	TArray<FString> Out;

	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *BlueprintPath);
	if (!WBP)
	{
		Out.Add(FString::Printf(TEXT("FAIL could not load widget blueprint %s"), *BlueprintPath));
		return Out;
	}
	if (!WBP->WidgetTree)
	{
		Out.Add(FString::Printf(TEXT("FAIL %s has no WidgetTree"), *BlueprintPath));
		return Out;
	}

	// Resolve the DESTINATION FIRST. If the target style does not exist there is nothing safe to do,
	// and discovering that after clearing half the references would leave the surface worse than it
	// started -- an unstyled widget is a harder defect to see than a wrongly-styled one.
	UClass* ToClass = LoadObject<UClass>(nullptr, *ToStyleObjectPath);
	if (!ToClass)
	{
		Out.Add(FString::Printf(
			TEXT("FAIL destination style does not exist: %s -- nothing changed. If this is a newly "
			     "compiled style, recompile the design page before repointing."),
			*ToStyleObjectPath));
		return Out;
	}

	int32 Matched = 0;
	TArray<UWidget*> Changed;

	WBP->WidgetTree->ForEachWidget([&](UWidget* W)
	{
		if (!W)
		{
			return;
		}

		// BY REFLECTION, not an accessor: CommonUI keeps 'Style' as a protected UPROPERTY and 5.6
		// publishes no getter for it. This is the same read the auditor makes, deliberately -- if the
		// two disagreed about what "the style" is, the verification would be meaningless.
		FClassProperty* SP = CastField<FClassProperty>(W->GetClass()->FindPropertyByName(TEXT("Style")));
		if (!SP)
		{
			return;
		}

		UObject* CurVal = SP->GetPropertyValue_InContainer(W);
		UClass* CurClass = Cast<UClass>(CurVal);
		if (!CurClass)
		{
			return;
		}

		// Compare without the trailing _C so the caller passes the name shown in the content browser.
		FString CurName = CurClass->GetName();
		CurName.RemoveFromEnd(TEXT("_C"));
		if (CurName != FromStyleClassName)
		{
			return;
		}

		++Matched;
		Out.Add(FString::Printf(TEXT("  %-28s %s -> %s"),
			*W->GetName(), *CurName, *ToClass->GetName()));

		if (bApply)
		{
			W->Modify();
			SP->SetPropertyValue_InContainer(W, ToClass);
			Changed.Add(W);
		}
	});

	if (Matched == 0)
	{
		Out.Add(FString::Printf(TEXT("  (no widget in %s uses %s)"),
			*WBP->GetName(), *FromStyleClassName));
		return Out;
	}

	if (!bApply)
	{
		Out.Add(FString::Printf(TEXT("DRY RUN %s: %d widget(s) would move off %s. Nothing written."),
			*WBP->GetName(), Matched, *FromStyleClassName));
		return Out;
	}

	// COMPILE, then save. A scripted edit to a blueprint's templates that is not compiled leaves the
	// generated class stale, and the surface then renders from the old style while the asset on disk
	// claims the new one -- a divergence that only shows up at runtime.
	WBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	FKismetEditorUtilities::CompileBlueprint(WBP);

	TArray<UPackage*> ToSave;
	ToSave.Add(WBP->GetOutermost());
	const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(ToSave, /*bOnlyDirty*/ false);

	Out.Add(FString::Printf(
		TEXT("APPLIED %s: %d widget(s) moved onto %s, compiled, save=%s"),
		*WBP->GetName(), Matched, *ToClass->GetName(), bSaved ? TEXT("OK") : TEXT("FAILED")));

	// Stated rather than implied: compiling is not proof. The blueprint compiler reports success in
	// cases this project has already been burned by, so the caller is expected to re-run the
	// READ-ONLY auditor against the saved asset and confirm the reference actually moved.
	Out.Add(TEXT("VERIFY by re-running UAFLWidgetAuditLibrary::AuditWidgetBlueprint -- a clean "
	             "compile here does not prove the reference changed on disk."));

	return Out;
}


TArray<FString> UAFLStyleRepointLibrary::RetintBorder(
	const FString& BlueprintPath,
	const FString& WidgetName,
	FLinearColor TokenColor,
	bool bApply)
{
	TArray<FString> Out;

	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *BlueprintPath);
	if (!WBP || !WBP->WidgetTree)
	{
		Out.Add(FString::Printf(TEXT("FAIL could not load %s (or it has no WidgetTree)"), *BlueprintPath));
		return Out;
	}

	UBorder* Target = nullptr;
	WBP->WidgetTree->ForEachWidget([&](UWidget* W)
	{
		if (W && W->GetName() == WidgetName)
		{
			Target = Cast<UBorder>(W);
		}
	});

	if (!Target)
	{
		// Named rather than silent: a typo here would otherwise read exactly like "already correct".
		Out.Add(FString::Printf(
			TEXT("FAIL %s has no Border named '%s' -- nothing changed."), *WBP->GetName(), *WidgetName));
		return Out;
	}

	const FLinearColor Before = Target->GetBrushColor();
	Out.Add(FString::Printf(TEXT("  %-16s rgba(%.3f,%.3f,%.3f,%.2f) -> rgba(%.3f,%.3f,%.3f,%.2f)"),
		*WidgetName, Before.R, Before.G, Before.B, Before.A,
		TokenColor.R, TokenColor.G, TokenColor.B, TokenColor.A));

	if (!bApply)
	{
		Out.Add(TEXT("DRY RUN -- nothing written."));
		return Out;
	}

	Target->Modify();
	Target->SetBrushColor(TokenColor);

	WBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	FKismetEditorUtilities::CompileBlueprint(WBP);

	TArray<UPackage*> ToSave;
	ToSave.Add(WBP->GetOutermost());
	const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(ToSave, /*bOnlyDirty*/ false);

	Out.Add(FString::Printf(TEXT("APPLIED %s.%s compiled, save=%s"),
		*WBP->GetName(), *WidgetName, bSaved ? TEXT("OK") : TEXT("FAILED")));
	return Out;
}


TArray<FString> UAFLStyleRepointLibrary::AddDepthScrim(
	const FString& BlueprintPath,
	FLinearColor DepthColor,
	bool bApply)
{
	static const FName ScrimName(TEXT("DepthGround"));
	TArray<FString> Out;

	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *BlueprintPath);
	if (!WBP || !WBP->WidgetTree)
	{
		Out.Add(FString::Printf(TEXT("FAIL could not load %s"), *BlueprintPath));
		return Out;
	}

	UWidget* Root = WBP->WidgetTree->RootWidget;
	UPanelWidget* RootPanel = Cast<UPanelWidget>(Root);
	if (!RootPanel)
	{
		Out.Add(FString::Printf(
			TEXT("FAIL root of %s is %s, not a panel -- cannot insert a child behind it."),
			*WBP->GetName(), *GetNameSafe(Root)));
		return Out;
	}

	// IDEMPOTENT: retint an existing scrim rather than adding a second one. Running this twice
	// should not stack two full-screen borders, which would be invisible in a screenshot and
	// obvious only as a rendering cost.
	UBorder* Existing = nullptr;
	WBP->WidgetTree->ForEachWidget([&](UWidget* W)
	{
		if (W && W->GetFName() == ScrimName) { Existing = Cast<UBorder>(W); }
	});

	Out.Add(FString::Printf(TEXT("root panel: %s (%s), %d child(ren)"),
		*RootPanel->GetName(), *RootPanel->GetClass()->GetName(), RootPanel->GetChildrenCount()));

	if (Existing)
	{
		Out.Add(TEXT("  DepthGround already present -- retinting, not duplicating."));
		if (!bApply) { Out.Add(TEXT("DRY RUN -- nothing written.")); return Out; }
		Existing->Modify();
		Existing->SetBrushColor(DepthColor);
	}
	else
	{
		Out.Add(FString::Printf(TEXT("  would insert DepthGround at index 0, rgba(%.3f,%.3f,%.3f,%.2f)"),
			DepthColor.R, DepthColor.G, DepthColor.B, DepthColor.A));
		if (!bApply) { Out.Add(TEXT("DRY RUN -- nothing written.")); return Out; }

		UBorder* Created = WBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), ScrimName);
		if (!Created)
		{
			Out.Add(TEXT("FAIL ConstructWidget returned null."));
			return Out;
		}
		Created->SetBrushColor(DepthColor);

		RootPanel->Modify();
		UPanelSlot* NewSlot = RootPanel->AddChild(Created);
		// AddChild appends; the scrim has to be UNDERNEATH everything, so move it to the back.
		RootPanel->ShiftChild(0, Created);

		// STRETCH IT. A child added to a CanvasPanel gets a default slot -- anchored top-left at a
		// fixed size -- so without this the "depth ground" is a small dark rectangle in a corner. The
		// insert would still report APPLIED, which is the failure worth naming: success here is a
		// full-bleed ground, not a widget that exists.
        (void)NewSlot;   // stretched below, on BOTH paths
	}

	// STRETCH ON BOTH PATHS -- insert AND retint.
	//
	// This lived in the insert branch only. DepthGround already existed by then, so every re-run took
	// the retint path, skipped the slot entirely, and still printed APPLIED. The scrim would have
	// stayed a small corner rectangle while the tool reported success -- a fix that reports done on
	// the strength of a branch that never ran. Re-derive the slot from the widget rather than reusing
	// a pointer from one branch.
	UBorder* Scrim = nullptr;
	WBP->WidgetTree->ForEachWidget([&](UWidget* W)
	{
		if (W && W->GetFName() == ScrimName) { Scrim = Cast<UBorder>(W); }
	});
	if (UCanvasPanelSlot* CanvasSlot = Scrim ? Cast<UCanvasPanelSlot>(Scrim->Slot) : nullptr)
	{
		CanvasSlot->Modify();
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));   // fill both axes
		CanvasSlot->SetOffsets(FMargin(0.f));                   // no inset on any edge
		CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
		CanvasSlot->SetZOrder(-100);                            // behind every sibling
		const FAnchors A = CanvasSlot->GetAnchors();
		Out.Add(FString::Printf(
			TEXT("  slot READ BACK: anchors (%.0f,%.0f)-(%.0f,%.0f), ZOrder %d -- full bleed"),
			A.Minimum.X, A.Minimum.Y, A.Maximum.X, A.Maximum.Y, CanvasSlot->GetZOrder()));
	}
	else
	{
		Out.Add(TEXT("  WARNING no CanvasPanelSlot found -- the scrim is NOT stretched."));
	}

	WBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	FKismetEditorUtilities::CompileBlueprint(WBP);

	TArray<UPackage*> ToSave;
	ToSave.Add(WBP->GetOutermost());
	const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);

	Out.Add(FString::Printf(TEXT("APPLIED DepthGround on %s, children now %d, save=%s"),
		*WBP->GetName(), RootPanel->GetChildrenCount(), bSaved ? TEXT("OK") : TEXT("FAILED")));
	return Out;
}

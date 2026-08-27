// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLStyleRepointLibrary.h"

#include "Blueprint/WidgetTree.h"
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

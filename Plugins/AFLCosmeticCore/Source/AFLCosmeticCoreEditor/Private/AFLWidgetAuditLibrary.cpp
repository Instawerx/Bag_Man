// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLWidgetAuditLibrary.h"

#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "CommonTextBlock.h"
#include "CommonBorder.h"
#include "UObject/UnrealType.h"

namespace
{
	/**
	 * The SSOT house palette, LINEAR (sec 2.1). Linear is canonical; the doc's hex is explicitly an
	 * approximation for design tools, so matching happens here in linear space.
	 */
	struct FHouseToken { const TCHAR* Name; FLinearColor Color; };

	static const TArray<FHouseToken>& HousePalette()
	{
		static const TArray<FHouseToken> P = {
			{ TEXT("House.Electric #1E5AFF"), FLinearColor(0.013f, 0.102f, 1.000f) },
			{ TEXT("House.Violet   #A855F7"), FLinearColor(0.400f, 0.090f, 0.930f) },
			{ TEXT("House.Black    #05080F"), FLinearColor(0.002f, 0.003f, 0.006f) },
			{ TEXT("House.White    #FFFFFF"), FLinearColor(1.000f, 1.000f, 1.000f) },
			{ TEXT("House.Cyan  DEPRECATED"), FLinearColor(0.000f, 0.940f, 1.000f) },
			{ TEXT("House.Blue    tertiary"), FLinearColor(0.000f, 0.420f, 1.000f) },
		};
		return P;
	}

	/** Nearest house token and its distance, ignoring alpha (alpha is a glass-tint concern, not identity). */
	static FString ClassifyColor(const FLinearColor& C)
	{
		const FHouseToken* Best = nullptr;
		float BestDist = TNumericLimits<float>::Max();
		for (const FHouseToken& T : HousePalette())
		{
			const float D = FMath::Abs(C.R - T.Color.R) + FMath::Abs(C.G - T.Color.G) + FMath::Abs(C.B - T.Color.B);
			if (D < BestDist) { BestDist = D; Best = &T; }
		}
		if (!Best) { return TEXT("unclassified"); }

		// A near-black or fully transparent value is not a palette choice -- reporting those as
		// "off-token" would bury the real findings in noise.
		if (C.A <= 0.001f)                       { return TEXT("transparent (no palette claim)"); }
		if (BestDist <= 0.02f)                   { return FString::Printf(TEXT("ON-TOKEN  %s"), Best->Name); }
		if (C.R < 0.03f && C.G < 0.03f && C.B < 0.03f) { return TEXT("near-black (depth, acceptable)"); }
		return FString::Printf(TEXT("OFF-TOKEN nearest %s  d=%.3f"), Best->Name, BestDist);
	}

	static void ReportColor(TArray<FString>& Out, const UWidget* W, const TCHAR* Prop, const FLinearColor& C)
	{
		Out.Add(FString::Printf(TEXT("  %-30s %-22s %-14s rgba(%.3f,%.3f,%.3f,%.2f)  %s"),
			*W->GetName(), *W->GetClass()->GetName(), Prop, C.R, C.G, C.B, C.A, *ClassifyColor(C)));
	}
}

TArray<FString> UAFLWidgetAuditLibrary::AuditWidgetBlueprint(const FString& BlueprintPath)
{
	TArray<FString> Out;

	UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *BlueprintPath);
	if (!WBP)
	{
		Out.Add(FString::Printf(TEXT("LOADFAIL %s"), *BlueprintPath));
		return Out;
	}
	if (!WBP->WidgetTree)
	{
		Out.Add(FString::Printf(TEXT("%s has no WidgetTree"), *BlueprintPath));
		return Out;
	}

	int32 Total = 0, OnToken = 0, OffToken = 0, Styled = 0;

	WBP->WidgetTree->ForEachWidget([&](UWidget* W)
	{
		if (!W) { return; }
		++Total;

		// STYLE REFERENCE FIRST. A widget driven by a compiled style is conformant BY CONSTRUCTION --
		// its colour comes from the token asset, so any literal sitting on it is inert and reporting it
		// as off-token would be a false finding.
		// BY REFLECTION, not an accessor: CommonUI keeps 'Style' as a protected UPROPERTY with no
		// public getter in 5.6 (GetStyleClass does not exist). Reflection also means a future CommonUI
		// widget that follows the same convention is covered without editing this file.
		if (W->IsA<UCommonTextBlock>() || W->IsA<UCommonBorder>())
		{
			UClass* StyleClass = nullptr;
			if (FClassProperty* SP = CastField<FClassProperty>(W->GetClass()->FindPropertyByName(TEXT("Style"))))
			{
				StyleClass = Cast<UClass>(SP->GetObjectPropertyValue_InContainer(W));
			}
			Out.Add(FString::Printf(TEXT("  %-30s %-22s STYLE          %s"),
				*W->GetName(), *W->GetClass()->GetName(),
				StyleClass ? *StyleClass->GetName() : TEXT("<<NONE -- falls back to literal>>")));
			if (StyleClass) { ++Styled; return; }
		}

		// Otherwise: read whatever colour properties this widget actually declares, by reflection, so a
		// widget type nobody anticipated still gets audited instead of silently passing.
		for (TFieldIterator<FProperty> It(W->GetClass()); It; ++It)
		{
			FProperty* P = *It;
			const FString PN = P->GetName();
			if (!PN.Contains(TEXT("Color")) && !PN.Contains(TEXT("Tint"))) { continue; }

			if (const FStructProperty* SP = CastField<FStructProperty>(P))
			{
				if (SP->Struct == TBaseStructure<FLinearColor>::Get())
				{
					const FLinearColor* C = SP->ContainerPtrToValuePtr<FLinearColor>(W);
					if (C) { ReportColor(Out, W, *PN, *C); }
				}
				else if (SP->Struct->GetFName() == FName(TEXT("SlateColor")))
				{
					// Reach INTO the struct for SpecifiedColor rather than calling GetSpecifiedColor():
					// that accessor inlines FSlateColor::GetColorFromTable, which is protected and not
					// exported from SlateCore, so it compiles fine and then fails at LINK.
					const void* SCPtr = SP->ContainerPtrToValuePtr<void>(W);
					if (const FStructProperty* Inner =
							CastField<FStructProperty>(SP->Struct->FindPropertyByName(TEXT("SpecifiedColor"))))
					{
						if (const FLinearColor* C = Inner->ContainerPtrToValuePtr<FLinearColor>(SCPtr))
						{
							ReportColor(Out, W, *PN, *C);
						}
					}
				}
			}
		}
	});

	// Recount from the emitted lines so the summary cannot disagree with the detail above it.
	for (const FString& L : Out)
	{
		if (L.Contains(TEXT("ON-TOKEN"))) { ++OnToken; }
		else if (L.Contains(TEXT("OFF-TOKEN"))) { ++OffToken; }
	}

	Out.Insert(FString::Printf(
		TEXT("%s -- %d widget(s): %d style-driven, %d on-token literal, %d OFF-TOKEN literal"),
		*FPaths::GetBaseFilename(BlueprintPath), Total, Styled, OnToken, OffToken), 0);
	return Out;
}

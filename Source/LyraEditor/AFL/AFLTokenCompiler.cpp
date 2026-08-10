// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLTokenCompiler.h"

#include "AFLDesignTokens.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CommonBorder.h"
#include "CommonTextBlock.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogAFLTokens, Log, All);

namespace
{
	/** The approved pages, in compile order. The lobby is LAST -- see CompileAllDesignPages' note. */
	const TCHAR* GApprovedPages[] = {
		TEXT("Docs/design/IRONICS_Home_Screen_Mockup.html"),
		TEXT("Docs/design/IRONICS_League_Door_Mockup.html"),
		TEXT("Docs/design/IRONICS_Staked_Door_Mockup.html"),
		TEXT("Docs/design/IRONICS_Lobby_Mockup.html"),
	};

	/**
	 * One emitted border style: a glass level from the page.
	 *
	 * The pages spell the same token two ways -- the home screen uses `--glass-1`, the lobby `--g1` -- because
	 * they were authored on different days. Both spellings are listed rather than one being "corrected" in
	 * the page, because editing an approved design to suit a parser inverts which artefact is authoritative.
	 */
	struct FBorderStyleSpec
	{
		const TCHAR* AssetName;
		TArray<FString> ColorTokens;
	};

	struct FTextStyleSpec
	{
		const TCHAR* AssetName;
		TArray<FString> ColorTokens;
	};

	const TArray<FBorderStyleSpec>& BorderSpecs()
	{
		static const TArray<FBorderStyleSpec> Specs = {
			{ TEXT("BS_IRONICS_Glass_Primary"),   { TEXT("--glass-1"), TEXT("--g1") } },
			{ TEXT("BS_IRONICS_Glass_Secondary"), { TEXT("--glass-2"), TEXT("--g2") } },
			{ TEXT("BS_IRONICS_Glass_Tertiary"),  { TEXT("--glass-3"), TEXT("--g3") } },
			{ TEXT("BS_IRONICS_Glass_Border"),    { TEXT("--glass-border"), TEXT("--gb") } },
			// The neon burns. One hue, four intensities -- this IS the one-blue rule expressed as assets.
			{ TEXT("BS_IRONICS_Neon_Resting"),    { TEXT("--neon") } },
			{ TEXT("BS_IRONICS_Neon_Rail"),       { TEXT("--neon-lit") } },
			{ TEXT("BS_IRONICS_Neon_Active"),     { TEXT("--neon-hot") } },
			{ TEXT("BS_IRONICS_Neon_Bloom"),      { TEXT("--neon-bloom") } },
		};
		return Specs;
	}

	const TArray<FTextStyleSpec>& TextSpecs()
	{
		static const TArray<FTextStyleSpec> Specs = {
			{ TEXT("TS_IRONICS_Text_Primary"),   { TEXT("--text-1"), TEXT("--t1") } },
			{ TEXT("TS_IRONICS_Text_Secondary"), { TEXT("--text-2"), TEXT("--t2") } },
			{ TEXT("TS_IRONICS_Text_Tertiary"),  { TEXT("--text-3"), TEXT("--t3") } },
			{ TEXT("TS_IRONICS_Text_Electric"),  { TEXT("--house-electric"), TEXT("--electric") } },
		};
		return Specs;
	}

	/** Every token the compiler reads, for the coverage report. */
	void CollectConsumedTokenNames(TSet<FString>& Out)
	{
		for (const FBorderStyleSpec& S : BorderSpecs()) { for (const FString& T : S.ColorTokens) { Out.Add(T); } }
		for (const FTextStyleSpec& S : TextSpecs())     { for (const FString& T : S.ColorTokens) { Out.Add(T); } }
		for (const TCHAR* T : { TEXT("--r-panel"), TEXT("--r-button"), TEXT("--r-input"), TEXT("--blur"),
		                        TEXT("--house-violet"), TEXT("--violet"), TEXT("--house-black"), TEXT("--depth") })
		{
			Out.Add(T);
		}
	}

	/**
	 * Find-or-create a Blueprint subclass of ParentClass, and hand back its CDO to be written.
	 *
	 * ⚠ UPDATE IN PLACE, NEVER RECREATE. Every widget that binds a style holds a TSubclassOf<> to this exact
	 * generated class. Deleting and remaking the asset on each run would null those references across the
	 * whole front end -- a re-run of the compiler would visually dismantle the UI it exists to keep correct.
	 */
	UObject* FindOrCreateStyleCDO(UClass* ParentClass, const FString& PackagePath, const FString& AssetName,
		bool bDryRun, bool& bOutCreated, UBlueprint*& OutBlueprint)
	{
		bOutCreated = false;
		OutBlueprint = nullptr;

		const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
		if (UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, *ObjectPath))
		{
			OutBlueprint = Existing;
			return Existing->GeneratedClass ? Existing->GeneratedClass->GetDefaultObject() : nullptr;
		}

		if (bDryRun)
		{
			bOutCreated = true;   // would create
			return nullptr;
		}

		const FString PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, FName(*AssetName), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (!NewBP)
		{
			return nullptr;
		}

		FAssetRegistryModule::AssetCreated(NewBP);
		Package->MarkPackageDirty();
		bOutCreated = true;
		OutBlueprint = NewBP;
		return NewBP->GeneratedClass ? NewBP->GeneratedClass->GetDefaultObject() : nullptr;
	}

	void SaveIfDirty(UBlueprint* BP)
	{
		if (!BP)
		{
			return;
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);

		if (UPackage* Package = BP->GetOutermost())
		{
			const FString FileName = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			UPackage::SavePackage(Package, nullptr, *FileName, Args);
		}
	}

	/**
	 * THE ONE-BLUE RULE, re-derived from what was actually emitted.
	 *
	 * The lobby page is explicit: "There is now exactly ONE blue in this lobby. UI.House.Blue #00ADFF is
	 * gone: its tertiary/inactive job is done by Electric turned down, not by a second hue." A second chrome
	 * hue is therefore a compile failure, not a warning -- #00ADFF is the FREE BASE IDENTITY, and letting it
	 * back into chrome makes furniture carry meaning that belongs to a player's identity.
	 *
	 * Checks HUE, not equality: a page that reintroduced #00ADFF under a new token name would pass a
	 * name-based check and still break the rule.
	 */
	/** Is this a saturated blue meaningfully distinct from Electric's hue? */
	bool IsSecondBlue(const FLinearColor& C, float ElectricHue, float& OutHue)
	{
		const FLinearColor HSV = C.LinearRGBToHSV();
		OutHue = HSV.R;
		// Greys and near-blacks carry no hue -- the glass ramp is white at low alpha and must not trip this.
		if (HSV.G < 0.25f || HSV.B < 0.05f)
		{
			return false;
		}
		const bool bIsBlueish = (HSV.R > 180.f && HSV.R < 260.f);
		return bIsBlueish && FMath::Abs(HSV.R - ElectricHue) > 12.f;
	}

	/**
	 * ⚠ THE VALIDATOR CHECKS THE ENGINE AGAINST THE DESIGN -- NEVER THE DESIGN AGAINST A RULE.
	 *
	 * The first version had this backwards and it showed immediately: it refused to compile the approved home
	 * screen because that page declares `--house-neonblue:#00ADFF`. But `UI.House.Blue #00ADFF` is a
	 * legitimate token in IRONICS_UI_STYLE_SSOT.md sec2.1 (tertiary fills, inactive states), and the page is
	 * the authority regardless. A compiler that rejects an approved design has inverted which artefact is the
	 * source of truth, and the only way to "fix" the failure would have been to edit the design to suit the
	 * parser.
	 *
	 * So the only thing worth asserting here is FIDELITY: does the colour we are about to write into a
	 * CommonUI asset actually equal the token the page declared? That is the failure this compiler exists to
	 * make impossible -- an sRGB value written where a linear one belongs, an alpha run through the gamma
	 * curve, a token silently resolving to black. The SSOT publishes the house colours in LINEAR alongside
	 * their hex, so those conversions can be checked against a number somebody already approved.
	 *
	 *     UI.House.Electric  #1E5AFF  (0.013, 0.102, 1.00)
	 *     UI.House.Violet    #A855F7  (0.40,  0.09,  0.93)
	 *
	 * Palette composition -- which blues a page uses, whether Violet stayed on the edge -- is a DESIGN
	 * decision, made in the pages and the SSOT. It is observed and reported here, never enforced.
	 */
	struct FKnownHouseColor
	{
		const TCHAR* Label;
		TArray<FString> Tokens;
		FLinearColor ExpectedLinear;   // as published in IRONICS_UI_STYLE_SSOT.md sec2.1
	};

	const TArray<FKnownHouseColor>& KnownHouseColors()
	{
		static const TArray<FKnownHouseColor> Known = {
			{ TEXT("UI.House.Electric"), { TEXT("--house-electric"), TEXT("--electric") }, FLinearColor(0.013f, 0.102f, 1.00f) },
			{ TEXT("UI.House.Violet"),   { TEXT("--house-violet"),   TEXT("--violet")   }, FLinearColor(0.40f,  0.09f,  0.93f) },
		};
		return Known;
	}

	/**
	 * Assert the parse+convert pipeline reproduces the SSOT's published linear values.
	 *
	 * A failure here means the COMPILER is wrong, which is the only thing this stage may fail on. Tolerance is
	 * generous because the SSOT rounds to 2-3 places -- it is catching a wrong colour space, not a rounding
	 * difference in the last digit.
	 */
	bool ValidateTokenFidelity(const FAFLDesignTokens& Tokens, TArray<FString>& OutViolations, TArray<FString>& OutNotes)
	{
		for (const FKnownHouseColor& Known : KnownHouseColors())
		{
			FLinearColor Parsed;
			if (!Tokens.TryGetColorAny(Known.Tokens, Parsed))
			{
				continue;   // not every page declares every house colour -- not this stage's business
			}
			const float Delta = FMath::Max3(
				FMath::Abs(Parsed.R - Known.ExpectedLinear.R),
				FMath::Abs(Parsed.G - Known.ExpectedLinear.G),
				FMath::Abs(Parsed.B - Known.ExpectedLinear.B));

			if (Delta > 0.02f)
			{
				OutViolations.Add(FString::Printf(
					TEXT("%s converts to linear (%.3f, %.3f, %.3f) but the style SSOT publishes ")
					TEXT("(%.3f, %.3f, %.3f) -- the COMPILER's colour conversion is wrong, not the page."),
					Known.Label, Parsed.R, Parsed.G, Parsed.B,
					Known.ExpectedLinear.R, Known.ExpectedLinear.G, Known.ExpectedLinear.B));
			}
			else
			{
				OutNotes.Add(FString::Printf(TEXT("%s fidelity OK -- linear (%.3f, %.3f, %.3f)"),
					Known.Label, Parsed.R, Parsed.G, Parsed.B));
			}
		}
		return OutViolations.Num() == 0;
	}

	/** Palette composition, OBSERVED AND REPORTED. Never fails a page -- the design decides the palette. */
	void ObservePalette(const FAFLDesignTokens& Tokens, TArray<FString>& OutNotes)
	{
		FLinearColor Electric;
		if (!Tokens.TryGetColorAny({ TEXT("--house-electric"), TEXT("--electric") }, Electric))
		{
			return;
		}
		const float ElectricHue = Electric.LinearRGBToHSV().R;

		for (const TPair<FString, FString>& Pair : Tokens.Raw)
		{
			FLinearColor C;
			float Hue = 0.f;
			if (Tokens.TryGetColor(Pair.Key, C) && IsSecondBlue(C, ElectricHue, Hue))
			{
				OutNotes.Add(FString::Printf(
					TEXT("palette: %s = %s is a second blue (hue %.1f vs Electric %.1f). The lobby page ")
					TEXT("collapsed chrome to one blue; noting where a page still carries the older vocabulary."),
					*Pair.Key, *Pair.Value, Hue, ElectricHue));
			}
		}
	}
}

bool UAFLTokenCompilerLibrary::CompileDesignPage(const FString& PageRelativePath, const FString& OutputPackagePath, bool bDryRun)
{
	const FString AbsPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / PageRelativePath);

	FAFLDesignTokens Tokens;
	FString Error;
	if (!FAFLDesignTokenParser::ParseFile(AbsPath, Tokens, Error))
	{
		UE_LOG(LogAFLTokens, Error, TEXT("AFL_TOKENS: %s"), *Error);
		return false;
	}
	UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS: %s -- %d token(s)%s"),
		*PageRelativePath, Tokens.Raw.Num(), bDryRun ? TEXT("  [DRY RUN]") : TEXT(""));

	// RESOLVE FIRST, VALIDATE, THEN WRITE. Resolution has to precede validation because the rule is about
	// what reaches chrome, and nothing is known to reach chrome until the specs are matched against this
	// page's vocabulary. Writing before validating would bake a violation into the very assets meant to
	// enforce the rule.
	TMap<FString, FLinearColor> BorderColors;
	TMap<FString, FLinearColor> TextColors;
	int32 Missing = 0;

	for (const FBorderStyleSpec& Spec : BorderSpecs())
	{
		FLinearColor Color;
		if (Tokens.TryGetColorAny(Spec.ColorTokens, Color)) { BorderColors.Add(Spec.AssetName, Color); }
		else { ++Missing; }
	}
	for (const FTextStyleSpec& Spec : TextSpecs())
	{
		FLinearColor Color;
		if (Tokens.TryGetColorAny(Spec.ColorTokens, Color)) { TextColors.Add(Spec.AssetName, Color); }
		else { ++Missing; }
	}

	// FIDELITY is the only thing that may fail: it says the compiler mis-converted a colour. Palette
	// composition is the design's call and is reported, not enforced.
	TArray<FString> Violations, Notes;
	const bool bFidelityOk = ValidateTokenFidelity(Tokens, Violations, Notes);
	ObservePalette(Tokens, Notes);
	for (const FString& N : Notes)
	{
		UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS:   %s"), *N);
	}
	if (!bFidelityOk)
	{
		for (const FString& V : Violations)
		{
			UE_LOG(LogAFLTokens, Error, TEXT("AFL_TOKENS: FIDELITY FAILURE -- %s"), *V);
		}
		return false;
	}

	int32 Written = 0, Created = 0;

	for (const FBorderStyleSpec& Spec : BorderSpecs())
	{
		const FLinearColor* Found = BorderColors.Find(Spec.AssetName);
		if (!Found)
		{
			// Not an error: no single page declares every token. Counted so a genuinely absent token is
			// visible rather than being quietly emitted as black.
			continue;
		}
		const FLinearColor Color = *Found;

		bool bCreated = false;
		UBlueprint* BP = nullptr;
		UObject* CDO = FindOrCreateStyleCDO(UCommonBorderStyle::StaticClass(), OutputPackagePath, Spec.AssetName,
			bDryRun, bCreated, BP);

		UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS:   %-32s %s  rgba(%.3f,%.3f,%.3f,%.3f)%s"),
			Spec.AssetName, bCreated ? TEXT("CREATE") : TEXT("update"),
			Color.R, Color.G, Color.B, Color.A, bDryRun ? TEXT("  [dry]") : TEXT(""));

		if (!bDryRun && CDO)
		{
			UCommonBorderStyle* Style = CastChecked<UCommonBorderStyle>(CDO);
			Style->Background.TintColor = FSlateColor(Color);
			Style->Background.DrawAs = ESlateBrushDrawType::RoundedBox;
			SaveIfDirty(BP);
			++Written;
		}
		Created += bCreated ? 1 : 0;
	}

	for (const FTextStyleSpec& Spec : TextSpecs())
	{
		const FLinearColor* Found = TextColors.Find(Spec.AssetName);
		if (!Found)
		{
			continue;
		}
		const FLinearColor Color = *Found;

		bool bCreated = false;
		UBlueprint* BP = nullptr;
		UObject* CDO = FindOrCreateStyleCDO(UCommonTextStyle::StaticClass(), OutputPackagePath, Spec.AssetName,
			bDryRun, bCreated, BP);

		UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS:   %-32s %s  rgba(%.3f,%.3f,%.3f,%.3f)%s"),
			Spec.AssetName, bCreated ? TEXT("CREATE") : TEXT("update"),
			Color.R, Color.G, Color.B, Color.A, bDryRun ? TEXT("  [dry]") : TEXT(""));

		if (!bDryRun && CDO)
		{
			UCommonTextStyle* Style = CastChecked<UCommonTextStyle>(CDO);
			Style->Color = Color;
			SaveIfDirty(BP);
			++Written;
		}
		Created += bCreated ? 1 : 0;
	}

	// Geometry is read and reported but NOT yet emitted: radii and blur belong on the brushes and on
	// BackgroundBlur widgets respectively, and writing them needs the widget tree that consumes them. Logged
	// so the numbers are visible now and nobody re-derives them by eye from the page later.
	float RPanel = 0.f, RButton = 0.f, RInput = 0.f, Blur = 0.f;
	Tokens.TryGetScalar(TEXT("--r-panel"), RPanel);
	Tokens.TryGetScalar(TEXT("--r-button"), RButton);
	Tokens.TryGetScalar(TEXT("--r-input"), RInput);
	Tokens.TryGetScalar(TEXT("--blur"), Blur);
	if (RPanel > 0.f || Blur > 0.f)
	{
		UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS:   geometry -- panel=%.0f button=%.0f input=%.0f blur=%.0f"),
			RPanel, RButton, RInput, Blur);
	}

	UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS: %s -- %d written, %d new, %d token(s) absent from this page%s"),
		*PageRelativePath, Written, Created, Missing, bDryRun ? TEXT("  [DRY RUN -- nothing saved]") : TEXT(""));
	return true;
}

bool UAFLTokenCompilerLibrary::CompileAllDesignPages(bool bDryRun)
{
	const FString OutputPath = TEXT("/Game/UI/IRONICS/Styles");
	bool bAllOk = true;

	// LOBBY LAST, DELIBERATELY. It is the page that collapsed the palette to a single blue and retired
	// #00ADFF from chrome; where an earlier page still carries the older vocabulary, the newer ruling has to
	// be the one that lands in the asset.
	for (const TCHAR* Page : GApprovedPages)
	{
		const FString Rel = Page;
		if (!FPaths::FileExists(FPaths::ProjectDir() / Rel))
		{
			UE_LOG(LogAFLTokens, Warning, TEXT("AFL_TOKENS: page not on disk, skipped -- %s"), *Rel);
			continue;
		}
		bAllOk &= CompileDesignPage(Rel, OutputPath, bDryRun);
	}
	return bAllOk;
}

bool UAFLTokenCompilerLibrary::ReportTokenCoverage(const FString& PageRelativePath)
{
	const FString AbsPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / PageRelativePath);

	FAFLDesignTokens Tokens;
	FString Error;
	if (!FAFLDesignTokenParser::ParseFile(AbsPath, Tokens, Error))
	{
		UE_LOG(LogAFLTokens, Error, TEXT("AFL_TOKENS: %s"), *Error);
		return false;
	}

	TSet<FString> Consumed;
	CollectConsumedTokenNames(Consumed);

	TArray<FString> Unconsumed;
	for (const TPair<FString, FString>& Pair : Tokens.Raw)
	{
		if (!Consumed.Contains(Pair.Key))
		{
			Unconsumed.Add(FString::Printf(TEXT("%s = %s"), *Pair.Key, *Pair.Value));
		}
	}
	Unconsumed.Sort();

	UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS: coverage for %s -- %d declared, %d consumed by the compiler"),
		*PageRelativePath, Tokens.Raw.Num(), Tokens.Raw.Num() - Unconsumed.Num());

	// Printed in full rather than counted. Each of these is a design decision the page made that the engine
	// currently ignores -- motion curves, gradients, the type stack -- and that list IS the remaining work.
	for (const FString& U : Unconsumed)
	{
		UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS:   not yet consumed: %s"), *U);
	}
	return true;
}

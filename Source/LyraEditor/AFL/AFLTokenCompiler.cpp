// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLTokenCompiler.h"

#include "AFLDesignTokens.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CommonBorder.h"
#include "CommonButtonBase.h"   // UCommonButtonStyle -- the per-state brush set
#include "CommonTextBlock.h"
#include "Engine/Font.h"
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
	 * Does this token paint the INSIDE of a box or its EDGE?
	 *
	 * ⚠ NOT COSMETIC BOOKKEEPING -- IT WAS A FIDELITY BUG. Every neon burn was being written to
	 * `Background.TintColor`, which FILLS the box with translucent blue. The page uses them as
	 * `border:1px solid` in every single case: *"every structural edge on the screen is Electric at 40%
	 * (unlit) / 70% (rail) / 95% (lit segment)"*. A filled panel at 40% blue and an outlined one at 40% blue
	 * are completely different screens, and the compiler was confidently emitting the wrong one.
	 */
	enum class EBrushRole : uint8
	{
		/** Glass levels -- `background:rgba(255,255,255,.12)`. Paints the interior. */
		Fill,
		/** Neon burns and Glass.Border -- `border:1px solid`. Paints the rim, interior stays clear. */
		Outline
	};

	/** Which geometry token rounds this box. The pages declare panel 20 / button 12 / input 8. */
	enum class ERadiusRole : uint8 { Panel, Button, Input };

	/**
	 * One emitted border style.
	 *
	 * The pages spell the same token two ways -- the home screen uses `--glass-1`, the lobby `--g1` -- because
	 * they were authored on different days. Both spellings are listed rather than one being "corrected" in
	 * the page, because editing an approved design to suit a parser inverts which artefact is authoritative.
	 */
	struct FBorderStyleSpec
	{
		const TCHAR* AssetName;
		TArray<FString> ColorTokens;
		EBrushRole Role;
		ERadiusRole Radius;
	};

	struct FTextStyleSpec
	{
		const TCHAR* AssetName;
		TArray<FString> ColorTokens;
	};

	const TArray<FBorderStyleSpec>& BorderSpecs()
	{
		static const TArray<FBorderStyleSpec> Specs = {
			// Glass LEVELS fill; they are the panel interiors, at panel radius.
			{ TEXT("BS_IRONICS_Glass_Primary"),   { TEXT("--glass-1"), TEXT("--g1") }, EBrushRole::Fill,    ERadiusRole::Panel },
			{ TEXT("BS_IRONICS_Glass_Secondary"), { TEXT("--glass-2"), TEXT("--g2") }, EBrushRole::Fill,    ERadiusRole::Panel },
			{ TEXT("BS_IRONICS_Glass_Tertiary"),  { TEXT("--glass-3"), TEXT("--g3") }, EBrushRole::Fill,    ERadiusRole::Button },
			// Glass.BORDER is an edge despite the family name -- `Glass.Border | every panel edge`.
			{ TEXT("BS_IRONICS_Glass_Border"),    { TEXT("--glass-border"), TEXT("--gb") }, EBrushRole::Outline, ERadiusRole::Panel },
			// The neon burns. One hue, four intensities -- this IS the one-blue rule expressed as assets, and
			// every one of them is a RIM. Button radius: they edge controls (tiles, presets, tabs, the CTA).
			{ TEXT("BS_IRONICS_Neon_Resting"),    { TEXT("--neon") },       EBrushRole::Outline, ERadiusRole::Button },
			{ TEXT("BS_IRONICS_Neon_Rail"),       { TEXT("--neon-lit") },   EBrushRole::Outline, ERadiusRole::Button },
			{ TEXT("BS_IRONICS_Neon_Active"),     { TEXT("--neon-hot") },   EBrushRole::Outline, ERadiusRole::Button },
			{ TEXT("BS_IRONICS_Neon_Bloom"),      { TEXT("--neon-bloom") }, EBrushRole::Outline, ERadiusRole::Panel },
			// SEMANTIC, and deliberately NOT part of the accent. It appears exactly once -- the stake field
			// when the typed value falls outside every band -- and the page is explicit that "semantic colour
			// is not the accent", which is why red survives in a screen that otherwise has one hue.
			// Input radius: it edges the numeric field, the only 8px control on the surface.
			{ TEXT("BS_IRONICS_Tint_Danger"),     { TEXT("--danger") },     EBrushRole::Fill,    ERadiusRole::Input },
		};
		return Specs;
	}

	/** The three geometry values, read once per page. Zero means the page did not declare one. */
	struct FTokenGeometry
	{
		float Panel = 0.f;
		float Button = 0.f;
		float Input = 0.f;
		float Blur = 0.f;

		float For(ERadiusRole Role) const
		{
			switch (Role)
			{
			case ERadiusRole::Button: return Button;
			case ERadiusRole::Input:  return Input;
			default:                  return Panel;
			}
		}
	};

	FTokenGeometry ReadGeometry(const FAFLDesignTokens& Tokens)
	{
		FTokenGeometry G;
		Tokens.TryGetScalar(TEXT("--r-panel"), G.Panel);
		Tokens.TryGetScalar(TEXT("--r-button"), G.Button);
		Tokens.TryGetScalar(TEXT("--r-input"), G.Input);
		Tokens.TryGetScalar(TEXT("--blur"), G.Blur);
		return G;
	}

	/**
	 * Geometry resolved across ALL approved pages, not just the one being compiled.
	 *
	 * ⚠ THIS EXISTS BECAUSE "LOBBY LAST WINS" IS RIGHT FOR COLOUR AND WRONG FOR GEOMETRY. The lobby page
	 * declares no `--r-*` at all -- it writes its radii inline in the CSS rules -- so compiling it last with
	 * a per-page read zeroed every corner the door pages had just rounded, and emitted the buttons square.
	 * Caught by the run log reading `radius=0`.
	 *
	 * An ABSENT token is not the value zero. The three pages that do declare geometry agree exactly
	 * (20 / 12 / 8 / 28), so it is a house constant: take the last page that actually states each value.
	 */
	const FTokenGeometry& HouseGeometry()
	{
		static FTokenGeometry Resolved;
		static bool bDone = false;
		if (bDone)
		{
			return Resolved;
		}
		bDone = true;

		for (const TCHAR* Page : GApprovedPages)
		{
			const FString Abs = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / FString(Page));
			FAFLDesignTokens Tokens;
			FString Error;
			if (!FAFLDesignTokenParser::ParseFile(Abs, Tokens, Error))
			{
				continue;
			}
			const FTokenGeometry G = ReadGeometry(Tokens);
			if (G.Panel  > 0.f) { Resolved.Panel  = G.Panel;  }
			if (G.Button > 0.f) { Resolved.Button = G.Button; }
			if (G.Input  > 0.f) { Resolved.Input  = G.Input;  }
			if (G.Blur   > 0.f) { Resolved.Blur   = G.Blur;   }
		}
		return Resolved;
	}

	/**
	 * Shape one brush from a token colour, a role and a radius.
	 *
	 * A radius of 0 means the page declared none -- the brush then stays a plain Box rather than a
	 * RoundedBox with square corners, so "no geometry on this page" and "deliberately square" stay
	 * distinguishable in the asset.
	 */
	void ShapeBrush(FSlateBrush& Brush, const FLinearColor& Color, EBrushRole Role, float Radius)
	{
		if (Radius > 0.f)
		{
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		}

		if (Role == EBrushRole::Outline)
		{
			// 1px, matching the page's `border:1px solid` everywhere. The INTERIOR IS CLEARED: an outline
			// token that also tinted the fill would double-paint every nested panel.
			Brush.TintColor = FSlateColor(FLinearColor::Transparent);
			Brush.OutlineSettings.Color = FSlateColor(Color);
			Brush.OutlineSettings.Width = 1.f;
		}
		else
		{
			Brush.TintColor = FSlateColor(Color);
			Brush.OutlineSettings.Width = 0.f;
		}
	}

	const TArray<FTextStyleSpec>& TextSpecs()
	{
		static const TArray<FTextStyleSpec> Specs = {
			{ TEXT("TS_IRONICS_Text_Primary"),   { TEXT("--text-1"), TEXT("--t1") } },
			{ TEXT("TS_IRONICS_Text_Secondary"), { TEXT("--text-2"), TEXT("--t2") } },
			{ TEXT("TS_IRONICS_Text_Tertiary"),  { TEXT("--text-3"), TEXT("--t3") } },
			{ TEXT("TS_IRONICS_Text_Electric"),  { TEXT("--house-electric"), TEXT("--electric") } },
			// The band readout when the value matches no band -- `outside all bands`. Paired with the tint
			// above; both are semantic, neither is the accent.
			{ TEXT("TS_IRONICS_Text_Danger"),    { TEXT("--danger-ink") } },
		};
		return Specs;
	}

	/**
	 * The three button roles the pages actually draw. Named for what they DO, not where they sit.
	 *
	 *   Lead     the CTA -- `background:var(--electric)`, electric bloom, violet only on hover
	 *   Control  segments, size tiles, stake presets -- glass fill + neon rim, violet on hover
	 *   Tab      the ruleset tabs -- transparent at rest, electric fill @16% when selected
	 *
	 * ⚠ VIOLET APPEARS ONLY IN THE HOVER AND SELECTED-HOVER STATES, on every one of them. That is the blend
	 * law, not a stylistic choice: *"Electric is core, fill, edge and glow; Violet is rim, focus and hover,
	 * and never touches readable core, fill or text."* The rim is size-gated ON at >=64px and core-dominant
	 * at <=32px -- and the lobby page notes **nothing in this lobby is 64px**, so violet appears NOWHERE at
	 * rest. Emitting it into a resting state would break the rule the whole palette is built on.
	 */
	enum class EButtonRole : uint8 { Lead, Control, Tab };

	struct FButtonStyleSpec
	{
		const TCHAR* AssetName;
		EButtonRole Role;
	};

	const TArray<FButtonStyleSpec>& ButtonSpecs()
	{
		static const TArray<FButtonStyleSpec> Specs = {
			{ TEXT("BTN_IRONICS_Lead"),    EButtonRole::Lead },
			{ TEXT("BTN_IRONICS_Control"), EButtonRole::Control },
			{ TEXT("BTN_IRONICS_Tab"),     EButtonRole::Tab },
		};
		return Specs;
	}

	/** Every token the compiler reads, for the coverage report. */
	void CollectConsumedTokenNames(TSet<FString>& Out)
	{
		for (const FBorderStyleSpec& S : BorderSpecs()) { for (const FString& T : S.ColorTokens) { Out.Add(T); } }
		for (const FTextStyleSpec& S : TextSpecs())     { for (const FString& T : S.ColorTokens) { Out.Add(T); } }
		for (const TCHAR* T : { TEXT("--r-panel"), TEXT("--r-button"), TEXT("--r-input"), TEXT("--blur"),
		                        TEXT("--house-violet"), TEXT("--violet"), TEXT("--house-black"), TEXT("--depth"),
		                        TEXT("--neon-text") })
		{
			Out.Add(T);
		}
	}

	/**
	 * Tokens that UMG structurally CANNOT take from a style asset. Reported separately from what is owed.
	 *
	 * A motion curve is a widget animation, a gradient is a material, and a font STACK is a list of family
	 * names with no UFont behind any of them. Listing these beside genuinely-unimplemented work made the
	 * coverage report read as a backlog of failures when most of it is not addressable here at all.
	 */
	bool IsStructurallyUnreachable(const FString& TokenName, const FString& Value, FString& OutWhy)
	{
		if (TokenName.StartsWith(TEXT("--board-")))
		{
			// ⚠ NOT THE GAME. The page draws TWO surfaces and says so: `.board` is the review surround,
			// theme-aware light+dark, and `.scr` is the shipping screen, hard-pinned dark. The `--board-*`
			// neutrals style the page a human reads the mockup ON. Emitting them would put the review
			// chrome's light-mode greys into the game, and reporting them as OWED implies they belong.
			OutWhy = TEXT("review surround (.board), not the game screen (.scr) -- never ships");
			return true;
		}
		if (TokenName.StartsWith(TEXT("--f-")))
		{
			OutWhy = TEXT("font stack -- a list of family names, not a UFont (see the type-ramp note)");
			return true;
		}
		if (Value.Contains(TEXT("gradient")))
		{
			OutWhy = TEXT("CSS gradient -- needs a material, not a brush tint");
			return true;
		}
		if (Value.Contains(TEXT("cubic-bezier")) || TokenName.Contains(TEXT("ease")) || TokenName.Contains(TEXT("dur")))
		{
			OutWhy = TEXT("motion curve -- a UMG widget animation, which has no scripting API");
			return true;
		}
		if (Value.Contains(TEXT("px ")) && Value.Contains(TEXT("rgba")))
		{
			OutWhy = TEXT("box-shadow / glow -- Slate has no shadow on a brush; needs an art asset");
			return true;
		}
		return false;
	}

	/**
	 * Where an APPROVED display face goes when the type ramp is ruled.
	 *
	 * Nothing ships here today. `IRONICS_UI_STYLE_SSOT.md` OPEN ITEM 1 has the ramp unapproved, and the home
	 * screen spec is explicit that the mockup's system stack must not become the shipping face. Drop a UFont
	 * at this path and the next compile applies it to every emitted text style -- no code change.
	 */
	const TCHAR* GFontAssetPath = TEXT("/Game/UI/IRONICS/Type/F_IRONICS_Display.F_IRONICS_Display");

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
	/**
	 * Apply the approved face to the emitted text styles, if one exists yet.
	 *
	 * ⚠ ABSENCE IS THE EXPECTED STATE AND IS REPORTED AS OWED, NOT AS AN ERROR. A missing font here is an
	 * operator decision that has not been made, not a compiler failure -- logging it as an error would train
	 * everyone to ignore the compiler's errors, which are reserved for "the compiler mis-converted a value".
	 */
	void ApplyApprovedFontIfPresent(const FAFLDesignTokens& Tokens, bool bDryRun)
	{
		FString Display;
		Tokens.TryGetString(TEXT("--f-display"), Display);

		UObject* Font = StaticLoadObject(UObject::StaticClass(), nullptr, GFontAssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Font)
		{
			UE_LOG(LogAFLTokens, Log,
				TEXT("AFL_TOKENS:   type -- NO approved face (ramp is OPEN ITEM 1). Page asks for '%s'; styles ")
				TEXT("carry colour only and inherit the face. Drop a UFont at %s to bind it."),
				Display.IsEmpty() ? TEXT("(no --f-display token)") : *Display, GFontAssetPath);
			return;
		}

		const UFont* AsFont = Cast<UFont>(Font);
		if (!AsFont)
		{
			UE_LOG(LogAFLTokens, Warning,
				TEXT("AFL_TOKENS:   type -- %s exists but is a %s, not a UFont. Not applied."),
				GFontAssetPath, *Font->GetClass()->GetName());
			return;
		}

		int32 Applied = 0;
		for (const FTextStyleSpec& Spec : TextSpecs())
		{
			const FString ObjectPath = FString::Printf(TEXT("/Game/UI/IRONICS/Styles/%s.%s_C"), Spec.AssetName, Spec.AssetName);
			UClass* StyleClass = LoadClass<UCommonTextStyle>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet, nullptr);
			if (!StyleClass || bDryRun)
			{
				continue;
			}
			UCommonTextStyle* Style = Cast<UCommonTextStyle>(StyleClass->GetDefaultObject());
			if (!Style)
			{
				continue;
			}
			Style->Font.FontObject = const_cast<UFont*>(AsFont);
			if (UBlueprint* BP = Cast<UBlueprint>(StyleClass->ClassGeneratedBy))
			{
				SaveIfDirty(BP);
			}
			++Applied;
		}
		UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS:   type -- approved face applied to %d text style(s)."), Applied);
	}

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

	// Geometry is read BEFORE the emit loops now: radii are written into the brushes rather than logged
	// beside them. Resolved across ALL approved pages -- see HouseGeometry for why a per-page read emitted
	// square buttons.
	const FTokenGeometry Geo = HouseGeometry();

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
			ShapeBrush(Style->Background, Color, Spec.Role, Geo.For(Spec.Radius));
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

	// ── BUTTON STYLES ────────────────────────────────────────────────────────────────────────────────
	//
	// Emitted only when the page carries the colours a button needs. The neon burns and Electric come from
	// the same tokens the border styles used, so a page missing them would produce a button with a black
	// rim rather than no button at all -- which is the failure mode worth avoiding.
	FLinearColor Electric, Violet, NeonRest, NeonHot, Glass2;
	const bool bHaveElectric = Tokens.TryGetColorAny({ TEXT("--house-electric"), TEXT("--electric") }, Electric);
	const bool bHaveViolet   = Tokens.TryGetColorAny({ TEXT("--house-violet"), TEXT("--violet") }, Violet);
	const bool bHaveNeon     = Tokens.TryGetColorAny({ TEXT("--neon") }, NeonRest);
	const bool bHaveHot      = Tokens.TryGetColorAny({ TEXT("--neon-hot") }, NeonHot);
	const bool bHaveGlass2   = Tokens.TryGetColorAny({ TEXT("--glass-2"), TEXT("--g2") }, Glass2);

	if (bHaveElectric && bHaveViolet && bHaveNeon && bHaveHot && bHaveGlass2)
	{
		const float R = Geo.Button > 0.f ? Geo.Button : Geo.Panel;

		for (const FButtonStyleSpec& Spec : ButtonSpecs())
		{
			bool bCreated = false;
			UBlueprint* BP = nullptr;
			UObject* CDO = FindOrCreateStyleCDO(UCommonButtonStyle::StaticClass(), OutputPackagePath,
				Spec.AssetName, bDryRun, bCreated, BP);

			UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS:   %-32s %s  radius=%.0f%s"),
				Spec.AssetName, bCreated ? TEXT("CREATE") : TEXT("update"), R,
				bDryRun ? TEXT("  [dry]") : TEXT(""));

			if (bDryRun || !CDO)
			{
				Created += bCreated ? 1 : 0;
				continue;
			}

			UCommonButtonStyle* Style = CastChecked<UCommonButtonStyle>(CDO);

			// ── SIZE AND PADDING, STRAIGHT OFF THE PAGE ──────────────────────────────────────────────
			//
			// ⚠ OMITTING THESE IS NOT A COSMETIC SHORTFALL -- IT BREAKS THE LAYOUT. Left at CommonUI's
			// defaults the controls came out roughly twice their designed width, and the staked door's axis
			// row -- which carries FOUR groups (denomination, venue, size, stake) rather than the league
			// door's three -- OVERFLOWED 1280px horizontally, pushing the stake field off-screen. Caught in
			// the designer render, not by any compile.
			//
			// The page states each of these directly, so they are read rather than tuned:
			//   .cta    { height:42px; padding:0 28px }
			//   .size   { height:34px; padding:0 16px }   (also .preset, .btn)
			//   .tab    { padding:0 26px }  -- fills the 48px tab row
			switch (Spec.Role)
			{
			case EButtonRole::Lead:
				Style->ButtonPadding = FMargin(28.f, 0.f, 28.f, 0.f);
				Style->MinHeight = 42;
				break;
			case EButtonRole::Control:
				Style->ButtonPadding = FMargin(16.f, 0.f, 16.f, 0.f);
				Style->MinHeight = 34;
				break;
			case EButtonRole::Tab:
				Style->ButtonPadding = FMargin(26.f, 0.f, 26.f, 0.f);
				Style->MinHeight = 48;
				break;
			}

			// Selected on every role is Electric FILL -- "Electric = core/fill/active". Hover on every role
			// is a VIOLET RIM over the resting fill -- "Violet = rim/edge/focus/hover, never touching
			// readable core, fill or text". Disabled is 40% opacity with no glow, per the states tables.
			const FLinearColor SelectedFill = Spec.Role == EButtonRole::Tab
				? FLinearColor(Electric.R, Electric.G, Electric.B, 0.16f)   // `.tab[aria-selected]` @16%
				: FLinearColor(Electric.R, Electric.G, Electric.B, 0.22f);  // `.size[aria-pressed]` @22%

			switch (Spec.Role)
			{
			case EButtonRole::Lead:
				// `.cta` -- electric fill at rest. The bloom is a box-shadow Slate cannot express on a
				// brush; the fill and the rim carry it until an art asset lands.
				ShapeBrush(Style->NormalBase,      Electric, EBrushRole::Fill,    R);
				ShapeBrush(Style->NormalHovered,   Violet,   EBrushRole::Outline, R);
				Style->NormalHovered.TintColor = FSlateColor(Electric);   // violet RIM over the electric core
				ShapeBrush(Style->NormalPressed,   Electric, EBrushRole::Fill,    R);
				ShapeBrush(Style->SelectedBase,    Electric, EBrushRole::Fill,    R);
				ShapeBrush(Style->SelectedHovered, Violet,   EBrushRole::Outline, R);
				Style->SelectedHovered.TintColor = FSlateColor(Electric);
				ShapeBrush(Style->SelectedPressed, Electric, EBrushRole::Fill,    R);
				ShapeBrush(Style->Disabled,    FLinearColor(Electric.R, Electric.G, Electric.B, 0.40f),
					EBrushRole::Fill, R);
				break;

			case EButtonRole::Control:
				// `.size` / `.preset` / `.btn` -- glass fill with an unlit neon rim, violet rim on hover,
				// electric fill + electric rim when pressed-in.
				ShapeBrush(Style->NormalBase,      NeonRest, EBrushRole::Outline, R);
				Style->NormalBase.TintColor = FSlateColor(Glass2);
				ShapeBrush(Style->NormalHovered,   Violet,   EBrushRole::Outline, R);
				Style->NormalHovered.TintColor = FSlateColor(Glass2);
				ShapeBrush(Style->NormalPressed,   NeonHot,  EBrushRole::Outline, R);
				Style->NormalPressed.TintColor = FSlateColor(Glass2);
				ShapeBrush(Style->SelectedBase,    NeonHot,  EBrushRole::Outline, R);
				Style->SelectedBase.TintColor = FSlateColor(SelectedFill);
				ShapeBrush(Style->SelectedHovered, Violet,   EBrushRole::Outline, R);
				Style->SelectedHovered.TintColor = FSlateColor(SelectedFill);
				ShapeBrush(Style->SelectedPressed, NeonHot,  EBrushRole::Outline, R);
				Style->SelectedPressed.TintColor = FSlateColor(SelectedFill);
				ShapeBrush(Style->Disabled,    FLinearColor(NeonRest.R, NeonRest.G, NeonRest.B, 0.40f),
					EBrushRole::Outline, R);
				break;

			case EButtonRole::Tab:
				// `.tab` -- NOTHING at rest. The tab row's own baseline is the unlit tube; the tab draws no
				// box of its own until it is the lit segment.
				ShapeBrush(Style->NormalBase,      FLinearColor::Transparent, EBrushRole::Fill,    R);
				ShapeBrush(Style->NormalHovered,   Violet,                    EBrushRole::Outline, R);
				ShapeBrush(Style->NormalPressed,   SelectedFill,              EBrushRole::Fill,    R);
				ShapeBrush(Style->SelectedBase,    SelectedFill,              EBrushRole::Fill,    R);
				ShapeBrush(Style->SelectedHovered, Violet,                    EBrushRole::Outline, R);
				Style->SelectedHovered.TintColor = FSlateColor(SelectedFill);
				ShapeBrush(Style->SelectedPressed, SelectedFill,              EBrushRole::Fill,    R);
				ShapeBrush(Style->Disabled,    FLinearColor::Transparent, EBrushRole::Fill,    R);
				break;
			}

			// ── TEXT PER STATE ───────────────────────────────────────────────────────────────────────
			//
			// The page's own contrast note settles this: *"Electric on the black ground measures 3.75:1 --
			// under AA for 14px text. It is used for text in exactly TWO places, both INACTIVE states, where
			// the lit edge and the fill carry the signal. Every active and every readable value is white at
			// 18:1."* So Electric labels the resting Control and Tab, and white takes over the moment the
			// control is hovered or selected. The Lead is white throughout: it is never inactive.
			const auto StyleClassFor = [](const TCHAR* AssetName) -> TSubclassOf<UCommonTextStyle>
			{
				const FString Path = FString::Printf(TEXT("/Game/UI/IRONICS/Styles/%s.%s_C"), AssetName, AssetName);
				return LoadClass<UCommonTextStyle>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet, nullptr);
			};
			const TSubclassOf<UCommonTextStyle> TxtPrimary  = StyleClassFor(TEXT("TS_IRONICS_Text_Primary"));
			const TSubclassOf<UCommonTextStyle> TxtTertiary = StyleClassFor(TEXT("TS_IRONICS_Text_Tertiary"));
			const TSubclassOf<UCommonTextStyle> TxtElectric = StyleClassFor(TEXT("TS_IRONICS_Text_Electric"));

			const TSubclassOf<UCommonTextStyle> RestingText =
				(Spec.Role == EButtonRole::Lead) ? TxtPrimary : TxtElectric;

			if (RestingText) { Style->NormalTextStyle = RestingText; }
			if (TxtPrimary)
			{
				Style->NormalHoveredTextStyle   = TxtPrimary;
				Style->SelectedTextStyle        = TxtPrimary;
				Style->SelectedHoveredTextStyle = TxtPrimary;
			}
			if (TxtTertiary) { Style->DisabledTextStyle = TxtTertiary; }

			SaveIfDirty(BP);
			++Written;
			Created += bCreated ? 1 : 0;
		}
	}

	// ── BLUR ─────────────────────────────────────────────────────────────────────────────────────────
	//
	// ⚠ BLUR CANNOT BE EMITTED AS A STYLE, and that is a property of Slate rather than a gap here. There is
	// no blur field on FSlateBrush or on UCommonBorderStyle; the effect lives on a UBackgroundBlur widget's
	// BlurStrength, so it is set on the WIDGET TREE that consumes it. Reported with the radii so the number
	// is visible at compile time and nobody re-measures it off the page by eye.
	UE_LOG(LogAFLTokens, Log,
		TEXT("AFL_TOKENS:   geometry -- panel=%.0f button=%.0f input=%.0f  (blur=%.0f -> BackgroundBlur widget, not a style)"),
		Geo.Panel, Geo.Button, Geo.Input, Geo.Blur);

	// ── TYPE ─────────────────────────────────────────────────────────────────────────────────────────
	//
	// ⚠ NO FACE IS BAKED, DELIBERATELY, AND THIS IS NOT AN OVERSIGHT. The type ramp is OPEN ITEM 1 in the
	// style SSOT and is flagged unapproved in all three surface specs; the home screen spec says it in
	// terms that settle it: *"The mockup uses a system stack as a stand-in -- DO NOT INHERIT A SHIPPING
	// DISPLAY FACE FROM IT."* The pages name Bahnschrift / Segoe UI / Cascadia Mono, which are family names
	// with no UFont behind them, so there is nothing to point an FSlateFontInfo at even if the ramp were
	// approved.
	//
	// The hook is here and it is one line: drop an approved UFont at GFontAssetPath and every emitted text
	// style picks it up on the next run. Until then the styles carry colour only and inherit the face from
	// their parent, which is the correct behaviour for a stand-in.
	ApplyApprovedFontIfPresent(Tokens, bDryRun);

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

	// ⚠ THE LIST IS SPLIT, NOT JUST PRINTED, AND THAT IS THE POINT OF THIS PASS.
	//
	// The old report dumped every unconsumed token in one block. Most of them are things UMG structurally
	// cannot take from a style asset -- motion curves, gradients, box-shadows, font STACKS -- so the report
	// read as a long backlog of failures and was, predictably, ignored. Anything permanently unreachable
	// mixed in with real work makes the real work invisible.
	//
	// Three buckets now. Only the last one is a to-do list.
	TArray<FString> Unreachable, Owed;
	for (const TPair<FString, FString>& Pair : Tokens.Raw)
	{
		if (Consumed.Contains(Pair.Key))
		{
			continue;
		}
		FString Why;
		if (IsStructurallyUnreachable(Pair.Key, Pair.Value, Why))
		{
			Unreachable.Add(FString::Printf(TEXT("%-22s %s"), *Pair.Key, *Why));
		}
		else
		{
			Owed.Add(FString::Printf(TEXT("%s = %s"), *Pair.Key, *Pair.Value));
		}
	}
	Unreachable.Sort();
	Owed.Sort();

	const int32 ConsumedCount = Tokens.Raw.Num() - Unreachable.Num() - Owed.Num();
	UE_LOG(LogAFLTokens, Log,
		TEXT("AFL_TOKENS: coverage for %s -- %d declared | %d consumed | %d unreachable in UMG | %d OWED"),
		*PageRelativePath, Tokens.Raw.Num(), ConsumedCount, Unreachable.Num(), Owed.Num());

	// Named once each so the reason is on the record, then never again treated as work.
	for (const FString& U : Unreachable)
	{
		UE_LOG(LogAFLTokens, Verbose, TEXT("AFL_TOKENS:   unreachable: %s"), *U);
	}

	// THIS is the remaining work, and it is short enough to act on.
	for (const FString& O : Owed)
	{
		UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS:   OWED: %s"), *O);
	}
	if (Owed.Num() == 0)
	{
		UE_LOG(LogAFLTokens, Log, TEXT("AFL_TOKENS:   nothing owed -- every remaining token is unreachable in UMG."));
	}
	return true;
}

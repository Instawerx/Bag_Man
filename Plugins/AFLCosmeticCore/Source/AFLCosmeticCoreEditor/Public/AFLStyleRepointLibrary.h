// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AFLStyleRepointLibrary.generated.h"

/**
 * Repoint a Widget Blueprint's text-style references from one style asset to another.
 *
 * SEPARATE FROM UAFLWidgetAuditLibrary ON PURPOSE. That library's contract is that it writes
 * nothing, and it is the instrument used to VERIFY this one. Bolting a mutator onto the auditor
 * would leave the audit grading its own work -- the same class of mistake as a proof arm that
 * never exercises the shipping path. Two tools, one of which can only look.
 *
 * WHY THIS EXISTS
 * Three TS_IRONICS_Creator_* styles were authored outside AFLTokenCompiler because the creator
 * had no approved design page and needed a body-face 12pt style in primary ink that no spec
 * produced. Two of the three duplicate Display_Primary and Data_Primary exactly. They are shared
 * with the loadout, so they cannot simply be deleted: every referencer has to be moved onto a
 * compiler-emitted style first, or deletion leaves null styles behind.
 */
UCLASS()
class UAFLStyleRepointLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Point every widget in @BlueprintPath whose Style is @FromStyleClassName at @ToStyleObjectPath.
	 *
	 * @FromStyleClassName is matched against the style class name WITHOUT the trailing _C, so callers
	 * pass the asset name they see in the content browser.
	 * @ToStyleObjectPath is a full object path INCLUDING _C, e.g.
	 *   /Game/UI/IRONICS/Styles/TS_IRONICS_Text_Small.TS_IRONICS_Text_Small_C
	 *
	 * DEFAULTS TO A DRY RUN. The caller has to ask for the write explicitly. Returns one line per
	 * widget considered plus a trailing summary, so a dry run is a readable plan rather than a count.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|UI Repoint")
	static TArray<FString> RepointTextStyle(
		const FString& BlueprintPath,
		const FString& FromStyleClassName,
		const FString& ToStyleObjectPath,
		bool bApply = false);

	/**
	 * Set a named Border's BrushColor to a token value.
	 *
	 * EXISTS BECAUSE "ON-TOKEN" IS NOT THE SAME AS "CORRECTLY USED". The creator's three panel grounds
	 * were filled with House.Electric at 94%, and the colour audit passed them: Electric IS a house
	 * colour. It is the EDGE colour. As a panel fill it renders the whole surface as flat blue slabs and
	 * breaks the lobby page's contrast ruling, where Electric on black measures 3.75:1 and is permitted
	 * for text in exactly two inactive states. A palette check cannot see a role error, which is why the
	 * screen looked wrong while the audit read clean.
	 *
	 * These are plain UMG Borders, not UCommonBorders, so they cannot reference a BS_IRONICS_* style and
	 * the token value has to be written onto the brush directly.
	 *
	 * Dry-run by default, like RepointTextStyle. Verify with the READ-ONLY auditor afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|UI Repoint")
	static TArray<FString> RetintBorder(
		const FString& BlueprintPath,
		const FString& WidgetName,
		FLinearColor TokenColor,
		bool bApply = false);

	/**
	 * Insert a full-bleed depth scrim as the FIRST child of the root panel, behind everything.
	 *
	 * The design page puts glass panels on a `--depth` #05080F ground. In-game the creator has no such
	 * ground -- probed by name, thirteen candidates, none present -- so the front-end scene shows
	 * straight through and glass at 12% white washes out over a bright station interior. Glass is a
	 * LIFT off a dark ground; without the ground it is just haze.
	 *
	 * ADDITIVE and idempotent: if a Border of this name already exists it is retinted rather than
	 * duplicated, so running twice cannot stack two scrims.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|UI Repoint")
	static TArray<FString> AddDepthScrim(
		const FString& BlueprintPath,
		FLinearColor DepthColor,
		bool bApply = false);

	/**
	 * Create a Widget Blueprint under @PackagePath with @ParentClassPath as its parent, and populate
	 * it with the named widgets a C++ base expects to bind.
	 *
	 * EXISTS BECAUSE WidgetTree IS NOT REACHABLE FROM PYTHON and the asset-tools factory has a banked
	 * trap here: create_asset IGNORES parent_class, so a WBP authored that way silently parents to
	 * UUserWidget and every BindWidget on the intended base goes unfulfilled. The blueprint still
	 * compiles. This sets the parent explicitly and reports what it actually created.
	 *
	 * @WidgetSpecs is "Name=Type" per entry, e.g. "TierListContainer=VerticalBox". Types are resolved
	 * by name against the UMG widget classes; an unknown type is REPORTED and skipped rather than
	 * quietly omitted, because a missing bind target renders as an empty screen with no error.
	 *
	 * Returns one line per widget plus a summary. Verify with UAFLWidgetAuditLibrary afterwards --
	 * blueprint compilation reports success on unfulfilled BindWidgets, which this project has
	 * already been burned by.
	 */
	/**
	 * Add named widgets to an EXISTING Widget Blueprint, skipping any that are already there.
	 *
	 * SEPARATE FROM AuthorWidgetBlueprint because the creator already exists and carries the
	 * operator's dialled-in layout. Re-authoring it would discard that -- and this project has
	 * already lost hand-tuned placement once by rebuilding an asset instead of extending it.
	 *
	 * IDEMPOTENT: an existing widget of the same name is reported and left alone, never duplicated
	 * and never replaced. Running twice must not produce two of anything.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|UI Author")
	static TArray<FString> AddWidgetsToBlueprint(
		const FString& BlueprintPath,
		const TArray<FString>& WidgetSpecs,
		bool bApply = false);

	UFUNCTION(BlueprintCallable, Category = "AFL|UI Author")
	static TArray<FString> AuthorWidgetBlueprint(
		const FString& PackagePath,
		const FString& AssetName,
		const FString& ParentClassPath,
		const TArray<FString>& WidgetSpecs,
		bool bApply = false);
};

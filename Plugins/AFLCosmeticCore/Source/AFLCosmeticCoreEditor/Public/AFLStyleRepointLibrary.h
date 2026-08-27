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
};

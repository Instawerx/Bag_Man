// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "AFLTokenCompiler.generated.h"

/**
 * UAFLTokenCompilerLibrary -- compiles the approved design pages into concrete CommonUI style assets.
 *
 * ══ WHAT THIS IS FOR ══════════════════════════════════════════════════════════════════════════════════
 *
 * The IRONICS front end was designed as web pages (the home-screen split, the two doors, the S1/S2 lobby).
 * Those pages ARE the design -- not a reference to imitate. This compiler is how they become the shipping
 * UI: every colour, radius, blur and glass level is lifted out of their CSS mechanically and written into
 * real style assets, so the in-engine surface cannot drift from the approved page without the page changing
 * first.
 *
 * The alternative -- reading the mockups and hand-authoring styles to match -- is what this exists to stop.
 * A hand-copied palette is correct exactly once.
 *
 * ══ WHY IT EMITS BLUEPRINT SUBCLASSES AND NOT A DATA ASSET ════════════════════════════════════════════
 *
 * CommonUI does not read styling from a generic Data Asset at runtime. `UCommonTextStyle`,
 * `UCommonButtonStyle` and `UCommonBorderStyle` are all `UCLASS(Abstract, Blueprintable)`, and widgets bind a
 * TSubclassOf<> to them -- the style IS a class, resolved through its CDO. So a compiler that emitted a
 * DataTable of hex codes would produce something no CommonUI widget can consume.
 *
 * This therefore creates BLUEPRINT SUBCLASSES and writes their CDO defaults, which is the only shape that
 * binds. Assets are created once and thereafter UPDATED IN PLACE, so a re-run re-tints existing styles
 * rather than orphaning every widget that references them.
 *
 * ══ THE ONE-BLUE RULE IS ENFORCED, NOT DOCUMENTED ═════════════════════════════════════════════════════
 *
 * The lobby page states there is exactly one blue: every edge is Electric at a different burn, and the old
 * `#00ADFF` is gone from chrome because it is the FREE BASE IDENTITY and a third blue collides with it.
 * `ValidateOneBlueRule` re-derives that from the emitted assets and fails the run if a second chrome hue
 * appears -- because a rule that lives only in a comment is a rule that returns.
 */
UCLASS()
class UAFLTokenCompilerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Parse a design page and write/refresh its CommonUI style assets.
	 *
	 * @param PageRelativePath  Page to compile, relative to the project dir
	 *                          (e.g. "Docs/design/IRONICS_Home_Screen_Mockup.html").
	 * @param OutputPackagePath Content path the styles are written to (e.g. "/Game/UI/IRONICS/Styles").
	 * @param bDryRun           Report what WOULD be written and touch nothing. Default true: a compiler that
	 *                          writes assets on its first accidental invocation is a compiler nobody runs
	 *                          twice.
	 * @return                  True when every requested style compiled and validation passed.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "AFL|Design")
	static bool CompileDesignPage(const FString& PageRelativePath, const FString& OutputPackagePath, bool bDryRun = true);

	/**
	 * Compile every approved page in one pass -- the normal entry point.
	 *
	 * Order matters: the lobby page is compiled LAST and wins on conflict. It is the page that retired
	 * `#00ADFF` from chrome and collapsed the palette to a single blue, so where an earlier page still
	 * carries the older three-blue vocabulary, the newer ruling must be the one that lands.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "AFL|Design")
	static bool CompileAllDesignPages(bool bDryRun = true);

	/** Token names read but absent from the page, and page tokens nothing consumed. Neither is silent. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "AFL|Design")
	static bool ReportTokenCoverage(const FString& PageRelativePath);
};

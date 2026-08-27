// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AFLWidgetAuditLibrary.generated.h"

class UWidgetBlueprint;

/**
 * READ-ONLY audit of a Widget Blueprint's authored colours and style references.
 *
 * Exists because UWidgetBlueprint::WidgetTree is NOT exposed to Python (probed: "Failed to find
 * property 'widget_tree'"), and the AIK Lua bridge publishes ten functions, none of which touch UMG.
 * So the one question that matters for token conformance -- does this surface take its colour from a
 * compiled BS_IRONICS_/TS_IRONICS_ style, or from a literal somebody typed -- cannot be asked from
 * script at all.
 *
 * WRITES NOTHING. Every function here reads and reports. A conformance audit that can also mutate the
 * thing it audits is a tool that will eventually "fix" a surface nobody asked it to touch.
 */
UCLASS()
class UAFLWidgetAuditLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Walk every widget in the blueprint and report each authored colour and style reference.
	 *
	 * One line per finding, returned rather than logged so the caller owns the output. Colours are
	 * reported in LINEAR RGB -- the SSOT's canonical space -- because converting to hex here would
	 * invite comparison against the doc's "design-tool approximation" hex rather than the authoritative
	 * linear values.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|UI Audit")
	static TArray<FString> AuditWidgetBlueprint(const FString& BlueprintPath);
};

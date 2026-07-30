// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectPtr.h"

#include "AFLItemDefWiringLibrary.generated.h"

class UBlueprint;

/**
 * One (ItemDefinition Blueprint -> EquipmentDefinition class) wiring request.
 *
 * Writes ID_*.Fragments[<the UInventoryFragment_EquippableItem>].EquipmentDefinition, which is NOT
 * reachable from UE Python or the AIK bridge: UInventoryFragment_EquippableItem is a bare UCLASS() with
 * no BlueprintType, so Python has no generated binding and wraps the fragment as its base type
 * (ULyraInventoryItemFragment), then resolves the property against THAT type's map and fails with
 * "Failed to find property" even though the reflection data is present.
 */
USTRUCT(BlueprintType)
struct FAFLItemDefEquipmentWire
{
	GENERATED_BODY()

	/** The ID_* asset (a ULyraInventoryItemDefinition Blueprint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL")
	TObjectPtr<UBlueprint> ItemDefBlueprint = nullptr;

	/**
	 * The WID_* generated class to wire in.
	 *
	 * TSubclassOf<UObject> rather than TSubclassOf<ULyraEquipmentDefinition> is FORCED, not sloppy:
	 * ULyraEquipmentDefinition has no api export, so this module cannot name it. The type check therefore
	 * moves from compile time to an UNCONDITIONAL super-chain assertion in WireEquipmentDefinitions --
	 * a wrong class fails that pair loudly with a message and writes nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL")
	TSubclassOf<UObject> EquipmentDefinitionClass = nullptr;
};

/** Per-pair outcome, so a partial run is diagnosable rather than a single pass/fail. */
USTRUCT(BlueprintType)
struct FAFLItemDefWireResult
{
	GENERATED_BODY()

	/** Full path of the ItemDefinition Blueprint, or "<null>" when the request had none. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL")
	FString ItemDefPath;

	/** True when the fragment now holds the requested class (covers both "wired" and "already correct"). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL")
	bool bSuccess = false;

	/**
	 * True only when a write actually happened. Machine-checkable idempotency: an "already correct" pair
	 * reports bSuccess=true / bChanged=false and its package is NOT dirtied, so callers can count real
	 * writes without string-matching Message.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL")
	bool bChanged = false;

	/** Human-legible reason, e.g. "already correct", "no EquippableItem fragment". */
	UPROPERTY(BlueprintReadOnly, Category = "AFL")
	FString Message;
};

/**
 * Editor-only batch wiring for AFL item definitions.
 *
 * WRITE-ONLY BY DESIGN: mutates the Blueprint CDO and marks the package dirty. It does NOT compile and
 * does NOT save -- the caller does that on the existing verified path
 * (BlueprintEditorLibrary.compile_blueprint + EditorAssetLibrary.save_asset(only_if_is_dirty=False)).
 * That is what keeps this module free of any UnrealEd dependency.
 */
UCLASS()
class AFLCOSMETICCOREEDITOR_API UAFLItemDefWiringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Wire many (ID -> WID) pairs in one call. 126 single calls through the bridge is a chore; one call is
	 * a tool.
	 *
	 * Idempotent: a pair whose fragment already holds the requested class is reported "already correct" and
	 * neither Modify() nor MarkPackageDirty() is invoked for it. Safe to re-run over already-wired rows.
	 *
	 * @return one result per input pair, in input order.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Cosmetic Editor")
	static TArray<FAFLItemDefWireResult> WireEquipmentDefinitions(const TArray<FAFLItemDefEquipmentWire>& Pairs);
};

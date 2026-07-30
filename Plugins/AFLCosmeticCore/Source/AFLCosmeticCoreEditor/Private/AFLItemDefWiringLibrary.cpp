// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLItemDefWiringLibrary.h"

#include "AFLCosmeticCoreEditor.h"
#include "Engine/Blueprint.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace AFLItemDefWiring
{
	/** Native class names, matched by reflection because none of these types is api-exported. */
	static const TCHAR* EquippableFragmentClassName = TEXT("InventoryFragment_EquippableItem");
	static const TCHAR* EquipmentDefinitionBaseName = TEXT("LyraEquipmentDefinition");
	static const FName  FragmentsPropertyName(TEXT("Fragments"));
	static const FName  EquipmentDefPropertyName(TEXT("EquipmentDefinition"));

	/**
	 * EXACT class-name match anywhere in the super-chain.
	 *
	 * Exact, not Contains(): a substring test on "LyraEquipmentDefinition" would also accept a
	 * hypothetical "LyraEquipmentDefinitionOverride", which is precisely the mis-accept this assertion
	 * exists to prevent. The BP classes we are handed are named e.g. WID_AFL_HandCannon_SIMULARENT_R_C, so
	 * the match lands on the NATIVE base further up the chain -- hence walking the chain rather than
	 * testing the leaf.
	 */
	static bool ChainHasClassNamed(const UClass* Class, const TCHAR* ClassName)
	{
		for (const UClass* C = Class; C; C = C->GetSuperClass())
		{
			if (C->GetName() == ClassName)
			{
				return true;
			}
		}
		return false;
	}
}

TArray<FAFLItemDefWireResult> UAFLItemDefWiringLibrary::WireEquipmentDefinitions(
	const TArray<FAFLItemDefEquipmentWire>& Pairs)
{
	using namespace AFLItemDefWiring;

	TArray<FAFLItemDefWireResult> Results;
	Results.Reserve(Pairs.Num());

	int32 NumWired = 0;
	int32 NumAlready = 0;
	int32 NumFailed = 0;

	for (const FAFLItemDefEquipmentWire& Pair : Pairs)
	{
		FAFLItemDefWireResult Result;

		UBlueprint* ItemDefBlueprint = Pair.ItemDefBlueprint;
		Result.ItemDefPath = ItemDefBlueprint ? ItemDefBlueprint->GetPathName() : TEXT("<null>");

		// --- Fail this pair, log it, and move to the next. A partial run must stay legible. ---
		auto Fail = [&Result, &NumFailed](FString&& Why) -> void
		{
			Result.Message = MoveTemp(Why);
			++NumFailed;
			UE_LOG(LogAFLCosmeticCoreEditor, Error, TEXT("[ItemDefWire] FAILED %s -- %s"),
				*Result.ItemDefPath, *Result.Message);
		};

		if (!ItemDefBlueprint)
		{
			Fail(TEXT("ItemDefBlueprint is null"));
			Results.Add(Result);
			continue;
		}

		UClass* EquipmentClass = Pair.EquipmentDefinitionClass;
		if (!EquipmentClass)
		{
			Fail(TEXT("EquipmentDefinitionClass is null"));
			Results.Add(Result);
			continue;
		}

		// UNCONDITIONAL type assertion. EquipmentDefinitionClass is TSubclassOf<UObject> because the real
		// base cannot be named here, so this check IS the type system for that argument.
		if (!ChainHasClassNamed(EquipmentClass, EquipmentDefinitionBaseName))
		{
			Fail(FString::Printf(TEXT("class is not a ULyraEquipmentDefinition: '%s'"),
				*EquipmentClass->GetPathName()));
			Results.Add(Result);
			continue;
		}

		UClass* GeneratedClass = ItemDefBlueprint->GeneratedClass;
		if (!GeneratedClass)
		{
			Fail(TEXT("blueprint has no GeneratedClass (never compiled?)"));
			Results.Add(Result);
			continue;
		}

		UObject* ItemDefCdo = GeneratedClass->GetDefaultObject(/*bCreateIfNeeded*/ true);
		if (!ItemDefCdo)
		{
			Fail(TEXT("blueprint has no CDO"));
			Results.Add(Result);
			continue;
		}

		// Fragments lives on the native ULyraInventoryItemDefinition; FindFProperty walks the super-chain
		// from the BP generated class up to it.
		FArrayProperty* FragmentsProperty = FindFProperty<FArrayProperty>(GeneratedClass, FragmentsPropertyName);
		if (!FragmentsProperty)
		{
			Fail(TEXT("no 'Fragments' array property on the item definition"));
			Results.Add(Result);
			continue;
		}

		FObjectProperty* FragmentInner = CastField<FObjectProperty>(FragmentsProperty->Inner);
		if (!FragmentInner)
		{
			Fail(TEXT("'Fragments' inner is not an object property"));
			Results.Add(Result);
			continue;
		}

		UObject* EquippableFragment = nullptr;
		{
			FScriptArrayHelper FragmentsHelper(
				FragmentsProperty, FragmentsProperty->ContainerPtrToValuePtr<void>(ItemDefCdo));

			for (int32 Index = 0; Index < FragmentsHelper.Num(); ++Index)
			{
				UObject* Fragment = FragmentInner->GetObjectPropertyValue(FragmentsHelper.GetRawPtr(Index));
				if (Fragment && ChainHasClassNamed(Fragment->GetClass(), EquippableFragmentClassName))
				{
					EquippableFragment = Fragment;
					break;
				}
			}
		}

		if (!EquippableFragment)
		{
			Fail(TEXT("no EquippableItem fragment"));
			Results.Add(Result);
			continue;
		}

		// Fragments is an Instanced property, so a correctly-instanced fragment is outered to THIS CDO. If
		// it is not, the array is inherited by reference and writing would silently mutate a SHARED
		// archetype -- corrupting whatever else points at it. Refuse instead. (Same failure family as the
		// SubobjectData get_object archetype trap.) Cannot happen while every ID_* parents directly to the
		// native ULyraInventoryItemDefinition, but it becomes reachable the moment an ID_* parents to
		// another ID_* Blueprint.
		if (EquippableFragment->GetOuter() != ItemDefCdo)
		{
			Fail(FString::Printf(
				TEXT("fragment is a SHARED archetype (outer '%s' != this CDO) -- refusing to write"),
				*GetPathNameSafe(EquippableFragment->GetOuter())));
			Results.Add(Result);
			continue;
		}

		FClassProperty* EquipmentDefProperty =
			FindFProperty<FClassProperty>(EquippableFragment->GetClass(), EquipmentDefPropertyName);
		if (!EquipmentDefProperty)
		{
			Fail(FString::Printf(TEXT("no 'EquipmentDefinition' class property on '%s'"),
				*EquippableFragment->GetClass()->GetName()));
			Results.Add(Result);
			continue;
		}

		UObject* const ExistingValue =
			EquipmentDefProperty->GetObjectPropertyValue_InContainer(EquippableFragment);

		// IDEMPOTENT: already correct -> touch nothing. No Modify(), no MarkPackageDirty(), so re-running
		// the batch over wired rows leaves those packages clean and they do not show up as changed.
		if (ExistingValue == EquipmentClass)
		{
			Result.bSuccess = true;
			Result.bChanged = false;
			Result.Message = TEXT("already correct");
			++NumAlready;
			Results.Add(Result);
			continue;
		}

		EquippableFragment->Modify();
		ItemDefBlueprint->Modify();
		EquipmentDefProperty->SetObjectPropertyValue_InContainer(EquippableFragment, EquipmentClass);
		ItemDefBlueprint->MarkPackageDirty();

		// Read back through the same reflection path that wrote, so a silent no-op cannot be reported as a
		// success.
		UObject* const NewValue = EquipmentDefProperty->GetObjectPropertyValue_InContainer(EquippableFragment);
		if (NewValue != EquipmentClass)
		{
			Fail(FString::Printf(TEXT("write did not take -- reads back as '%s'"), *GetPathNameSafe(NewValue)));
			Results.Add(Result);
			continue;
		}

		Result.bSuccess = true;
		Result.bChanged = true;
		Result.Message = FString::Printf(TEXT("wired '%s' -> '%s' (was '%s')"),
			*GeneratedClass->GetName(), *EquipmentClass->GetName(), *GetNameSafe(ExistingValue));
		++NumWired;
		Results.Add(Result);

		UE_LOG(LogAFLCosmeticCoreEditor, Log, TEXT("[ItemDefWire] %s -- %s"),
			*Result.ItemDefPath, *Result.Message);
	}

	UE_LOG(LogAFLCosmeticCoreEditor, Log,
		TEXT("[ItemDefWire] batch complete: %d pair(s) -- %d wired, %d already correct, %d failed. ")
		TEXT("Caller must still compile + save the %d dirtied package(s)."),
		Pairs.Num(), NumWired, NumAlready, NumFailed, NumWired);

	return Results;
}

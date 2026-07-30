// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLCosmeticCoreEditor : ModuleRules
{
	public AFLCosmeticCoreEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Editor-only tooling for the cosmetic data layer. The FIRST Type=Editor module in AFL, so it never
		// ships and never loads in -game.
		//
		// Deliberately WRITE-ONLY -- it mutates a Blueprint CDO and marks the package dirty, and nothing else.
		// Compiling and SAVING stay on the already-verified Python path
		// (BlueprintEditorLibrary.compile_blueprint + EditorAssetLibrary.save_asset(only_if_is_dirty=False)),
		// which is why there is NO UnrealEd dependency here: no FKismetEditorUtilities, no SavePackage.
		//
		// There is also NO LyraGame dependency, and there CANNOT be one: ULyraInventoryItemDefinition
		// (UCLASS(Blueprintable, Const, Abstract)), ULyraEquipmentDefinition and
		// UInventoryFragment_EquippableItem all carry NO api export, so this module is unable to name any of
		// them. Every access is by reflection -- the same reason the QuickBar rail in AFLCombat walks class
		// NAMES (AFLSkinColorControllerComponent.cpp).
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine", // UBlueprint, UBlueprintFunctionLibrary
			}
		);
	}
}

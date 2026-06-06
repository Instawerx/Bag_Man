// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLCosmeticCore : ModuleRules
{
	public AFLCosmeticCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Early-loaded (Default phase, non-GameFeature) home for the cosmetic-economy CORE types + the
		// catalog. Kept deliberately thin: the catalog must be loadable when AssetManager scans
		// PrimaryAssetTypesToScan at engine startup, BEFORE any GameFeature module loads. The cosmetic
		// ASSETS it references stay soft (TSoftObjectPtr) and load on demand, so this module does NOT need
		// to depend on AFLCombat (which owns UAFLSkinColorAsset + the proven skin/controller code).
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",            // UPrimaryDataAsset, UGameInstanceSubsystem, AssetManager
				"GameplayTags",      // FGameplayTag in future entry/identity fields
				"GameplayAbilities", // UGameplayAbility (UAFLAbilityCosmeticAsset.AbilityClass soft-class ref)
			}
		);
	}
}

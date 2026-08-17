// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLCore : ModuleRules
{
	public AFLCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"LyraGame",
				"ModularGameplay",
				"ModularGameplayActors",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				// Cook guard (Cook/AFLCookedAssetRegistry.cpp): the EDITOR half asks the asset
				// registry for referencers, because in-editor "does the file exist" is always
				// true and therefore useless -- that is exactly why PIE never caught the
				// string-referenced-asset defect. AssetRegistry is a runtime module, so the
				// dependency is cook-safe; the call itself is behind WITH_EDITOR.
				"AssetRegistry",
			}
		);
	}
}

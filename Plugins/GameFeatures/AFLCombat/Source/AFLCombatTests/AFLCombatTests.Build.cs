// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLCombatTests : ModuleRules
{
	public AFLCombatTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AFLCombat",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayMessageRuntime",
				"FunctionalTesting",
				"LyraGame",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
			}
		);
	}
}

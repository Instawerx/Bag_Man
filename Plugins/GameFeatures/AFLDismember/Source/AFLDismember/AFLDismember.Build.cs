// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLDismember : ModuleRules
{
	public AFLDismember(ReadOnlyTargetRules Target) : base(Target)
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
				"NetCore",
				"GameplayTasks",
				"GameplayMessageRuntime",
				"LyraGame",
				"AFLCore",
				"AFLCombat",
				"ModularGameplay",
				"ModularGameplayActors",
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

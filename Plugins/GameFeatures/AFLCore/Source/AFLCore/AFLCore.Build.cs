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
			}
		);
	}
}

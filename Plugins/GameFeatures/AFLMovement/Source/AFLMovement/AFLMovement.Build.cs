// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLMovement : ModuleRules
{
	public AFLMovement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Sprint 3 Dash Movement Contract — see §9.6 of
		// Docs/BAG_MAN_MASTER_BUILD_v2.0.md. UAFLAttributeSet_Movement is
		// deferred to Sprint 4 (see §9.7); no AttributeSet deps here.
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
				"AFLCore",
				"AIModule",
				"MotionWarping",
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

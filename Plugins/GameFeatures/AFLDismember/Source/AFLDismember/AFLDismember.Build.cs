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
				// S4-INC1: Niagara for the FAFLDismemberZone.SparkFX soft slot (declared now,
				// consumed in the AFL-0406 sparks pass). Soft-ref only -- no hard NS load here.
				"Niagara",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				// S4-INC3 PHASE B-1: the head loot-box (AAFLHeadLootBox) wears UAFLGrabbableComponent
				// from AFLMovement (the grab substrate) + binds its OnGrabbedBy seam. AFLMovement does
				// NOT depend on AFLDismember, so this is non-circular. Used only in the .cpp (the header
				// forward-declares the component), hence Private.
				"AFLMovement",
			}
		);
	}
}

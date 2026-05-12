// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLMovement : ModuleRules
{
	public AFLMovement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Phase 1 scaffold — minimal deps for the module class only.
		// Phase 2 will add GameplayAbilities/GameplayTags/LyraGame/etc when
		// the Sprint 3 Dash Movement Contract runtime lands (see §9.6 of
		// Docs/BAG_MAN_MASTER_BUILD_v2.0.md). UAFLAttributeSet_Movement is
		// deferred to Sprint 4 (see §9.7).
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);
	}
}

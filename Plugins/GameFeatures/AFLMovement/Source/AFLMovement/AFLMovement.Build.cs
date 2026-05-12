// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLMovement : ModuleRules
{
	public AFLMovement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Phase 1 scaffold — minimal deps for the module class only.
		// Phase 2 will add GameplayAbilities/GameplayTags/LyraGame/etc when
		// UAFLAttributeSet_Movement code lands.
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

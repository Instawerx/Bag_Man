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
				// AFLW_HomeScreen.h derives from UCommonActivatableWidget, so including it to reach the
				// static R98 stake predicate pulls the CommonUI/UMG headers in with it. The test itself
				// constructs no widget -- it exercises a pure static -- but the include still needs these.
				"UMG",
				"CommonUI",
			}
		);
	}
}

// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLHub : ModuleRules
{
	public AFLHub(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"GameFeatures",
				"NetCore",
				"GameplayMessageRuntime",
				"LyraGame",
				// The seams the hub FRONTS (never re-implements): #43 cosmetic apply chain +
				// wallet ClientRequestPurchase live in AFLCombat (HUB-READ-1 s6, HUB-READ-2 s7).
				"AFLCombat",
				// AAFLWeaponSpawner + UAFLWeaponSpawnRegistry -- the proven pad pattern the
				// display pedestal subclasses (HUB-READ-1 s3). Always-loaded plugin, safe direction.
				"AFLGameCore",
				// PX retail: pedestal plates resolve name/price straight off the catalog (s4).
				"AFLCosmeticCore",
				// Doctrine: every net-serialised struct lives in an always-loaded module.
				"AFLNetTypes",
				"AFLVFX",
				"ModularGameplay",
				"ModularGameplayActors",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"CommonUI",
				"CommonGame",
				"UMG",
			}
		);
	}
}

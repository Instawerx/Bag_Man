// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLLiveOps : ModuleRules
{
	public AFLLiveOps(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Early-loaded (Default phase, non-GameFeature) home for the Battle Pass season vocabulary and
		// the per-player progress component. Kept thin for the same reason AFLCosmeticCore is: the
		// season asset must be loadable when AssetManager scans PrimaryAssetTypesToScan at engine
		// startup. Reward assets stay soft and load on demand.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",           // UPrimaryDataAsset, UActorComponent, AssetManager
				"GameplayTags",
				"AFLCosmeticCore",  // EAFLAcquisition::BattlePass -- the one existing hook (S21 scope)
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"NetCore",          // replication of the progress struct
			}
		);
	}
}

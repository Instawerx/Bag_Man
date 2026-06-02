// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLNetTypes : ModuleRules
{
	public AFLNetTypes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Cross-cutting REPLICATED types only. Kept deliberately thin so it can stay
		// always-loaded (Default phase, non-GameFeature) with no heavy coupling -- the
		// whole point is that these structs are in the FNetSerializeScriptStructCache
		// on every endpoint before any GameFeature loads.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayAbilities",   // FGameplayAbilityTargetData_SingleTargetHit base + TStructOpsTypeTraits
				"NetCore",             // FVector_NetQuantize / FVector_NetQuantizeNormal
			}
		);
	}
}

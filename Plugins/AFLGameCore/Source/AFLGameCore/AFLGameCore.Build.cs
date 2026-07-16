// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLGameCore : ModuleRules
{
	public AFLGameCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Always-loaded (Default phase, non-GameFeature) home for AFL game-framework classes that must
		// exist at world init. Kept thin. LyraGame is required by the ALyraGameMode base; the interface
		// seam (IAFLRoundRestartPolicy) carries NO dependency into any GameFeature.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"AIModule",   // FGenericTeamId (GenericTeamAgentInterface.h) for the team-assignment seam
				"LyraGame",   // ALyraGameMode + ULyraTeamCreationComponent bases (the always-loaded project module)
			}
		);
	}
}

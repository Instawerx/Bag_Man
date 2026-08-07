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
				"Json",       // T2: parse the GameLift matchmaker GameSessionData contract (UAFLMatchmakerDataProvider)
				"AIModule",   // FGenericTeamId (GenericTeamAgentInterface.h) for the team-assignment seam
				"GameplayAbilities",   // AI-3: send Event.Movement.Sprint.Requested; read State.Movement.Sprinting
				"GameplayMessageRuntime",   // UGameplayMessageSubsystem -- Lyra.Elimination.Message anti-camp feed (T1.4b-ii)
				"GameFeatures",   // UGameFeatureAction base + the FGameFeature*Context structs, for
				                  // UAFLGFA_ActivateDataLayers (district streaming). Declared directly (IWYU),
				                  // matching AFLCombat.Build.cs which needs it for UAFLGFA_WeaponSpawns.
				"LyraGame",   // ALyraGameMode + ULyraTeamCreationComponent bases (the always-loaded project module)
				"ModularGameplayActors",   // AModularAIController base of ALyraPlayerBotController (AAFLBotController's grandparent)
				// The signed server transport for the match-lifecycle posts (escrow/settle/rating).
				// AFLOnline is always-loaded (Default phase) like this module and depends on nothing here,
				// so the direction is one-way: game core USES the transport, the transport knows no game.
				"AFLOnline",
			}
		);
	}
}

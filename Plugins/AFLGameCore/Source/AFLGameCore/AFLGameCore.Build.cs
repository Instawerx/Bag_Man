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
				// S1 lobby: UAFLQueueDirectorySubsystem GETs /queues + /population. Declared DIRECTLY even
				// though AFLOnline already exposes HTTP publicly -- IWYU, and a transitive dependency that
				// disappears when AFLOnline's own deps are tidied is a build break nobody expects.
				"HTTP",
				"AIModule",   // FGenericTeamId (GenericTeamAgentInterface.h) for the team-assignment seam
				"GameplayAbilities",   // AI-3: send Event.Movement.Sprint.Requested; read State.Movement.Sprinting
				"GameplayMessageRuntime",   // UGameplayMessageSubsystem -- Lyra.Elimination.Message anti-camp feed (T1.4b-ii)
				"GameFeatures",   // UGameFeatureAction base + the FGameFeature*Context structs, for
				                  // UAFLGFA_ActivateDataLayers (district streaming). Declared directly (IWYU),
				                  // matching AFLCombat.Build.cs which needs it for UAFLGFA_WeaponSpawns.
				"CommonLoadingScreen",   // ILoadingProcessInterface -- UAFLDistrictReadinessComponent holds the
				                         // loading screen while a district's cells stream in. Lyra's own
				                         // ULyraExperienceManagerComponent implements the same contract.
				"ModularGameplay",   // UGameStateComponent base (ModularGameplay engine plugin -- NOT the
				                     // similarly-named ModularGameplayActors below, which supplies the modular
				                     // ACTORS). Declared directly (IWYU) as LyraGame and ModularGameplayActors
				                     // both do; it does not arrive transitively through them.
				"LyraGame",   // ALyraGameMode + ULyraTeamCreationComponent bases (the always-loaded project module)
				"ModularGameplayActors",   // AModularAIController base of ALyraPlayerBotController (AAFLBotController's grandparent)
				// The signed server transport for the match-lifecycle posts (escrow/settle/rating).
				// AFLOnline is always-loaded (Default phase) like this module and depends on nothing here,
				// so the direction is one-way: game core USES the transport, the transport knows no game.
				"AFLOnline",
			}
		);

		// S12: the GameLift Server SDK, for the onStartGameSession -> GetGameSessionData hop.
		//
		// SERVER TARGETS ONLY, and deliberately so. The vendored plugin declares TargetAllowList ["Server"]
		// on both its modules, so this dependency cannot be satisfied in an editor or client build -- adding
		// it unconditionally would break them. The SDK also defines WITH_GAMELIFT=1 only for server targets,
		// which is the macro UAFLGameLiftHostSubsystem compiles against; everywhere else that macro is absent
		// and the subsystem degrades to a no-op that leaves the ?MatchmakerData= launch option in charge.
		if (Target.Type == TargetRules.TargetType.Server)
		{
			PrivateDependencyModuleNames.Add("GameLiftServerSDK");
		}
	}
}

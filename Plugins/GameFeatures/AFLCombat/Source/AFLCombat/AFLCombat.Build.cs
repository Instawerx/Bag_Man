// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLCombat : ModuleRules
{
	public AFLCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"EnhancedInput",
				"GameplayAbilities",
				"GameplayTags",
				"NetCore",
				"GameplayTasks",
				"GameplayMessageRuntime",
				"LyraGame",
				"AFLCore",
				// AFL-0106 net fix: FAFLAbilityTargetData_Hitscan (the client-built hitscan
				// payload Pulse/Beam ship via ServerSetReplicatedTargetData) lives here, an
				// always-loaded non-GameFeature module, so it is in GAS's once-built
				// FNetSerializeScriptStructCache on every endpoint. It MUST NOT move back into
				// this GameFeature -- a late-loaded net-serialized struct desyncs the cache and
				// drops the connection (see AFLNetTypes.uplugin).
				"AFLNetTypes",
				"ModularGameplay",
				"ModularGameplayActors",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"Niagara",
				// AFL-0208: IAFLBeamEndpointProvider, implemented by UAFLBeamChannelComponent.
				// AFLVFX is the always-on root cosmetic plugin that owns the beam-cue contract;
				// AFLCombat (a GameFeature) depending on it is the correct load-order direction.
				// Private: only AFLCombat's own TUs compile the component that implements it.
				"AFLVFX",
				// AFL death system (1b): AAFLTargetDummy subclasses ALyraCharacter, which
				// implements IGenericTeamAgentInterface (FGenericTeamId / team-attitude). Those
				// symbols live in AIModule -- LyraGame links it for the same reason
				// (LyraGame.Build.cs). Without it: LNK2019 on the inherited interface thunks.
				"AIModule",
			}
		);
	}
}

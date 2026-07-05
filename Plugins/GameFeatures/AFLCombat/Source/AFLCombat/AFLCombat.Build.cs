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
				// Arena round wrapper: AAFLGameMode (the round respawn gate) was RELOCATED to the
				// always-loaded AFLGameCore plugin -- a map-default GameMode is instantiated at world init,
				// before this GameFeature loads, so it cannot live here. UAFLRoundManagerComponent
				// implements AFLGameCore's IAFLRoundRestartPolicy seam (GameFeature -> always-loaded, the
				// correct direction; AAFLGameMode keeps ZERO dependency back into AFLCombat).
				"AFLGameCore",
				// AFL-0106 net fix: FAFLAbilityTargetData_Hitscan (the client-built hitscan
				// payload Pulse/Beam ship via ServerSetReplicatedTargetData) lives here, an
				// always-loaded non-GameFeature module, so it is in GAS's once-built
				// FNetSerializeScriptStructCache on every endpoint. It MUST NOT move back into
				// this GameFeature -- a late-loaded net-serialized struct desyncs the cache and
				// drops the connection (see AFLNetTypes.uplugin).
				"AFLNetTypes",
				// S-ECON-CAT: the cosmetic catalog + its CORE types/subsystem live in the always-loaded
				// AFLCosmeticCore module (NOT this GameFeature) because AssetManager scans the catalog's
				// primary-asset type at engine startup, before GameFeatures load. AFLCombat depends on it
				// for the #43 resolve swap (UAFLCosmeticCatalogSubsystem) + EAFLCosmeticRarity (used by
				// UAFLSkinColorAsset). Correct dependency direction: GameFeature -> always-loaded core.
				"AFLCosmeticCore",
				"ModularGameplay",
				"ModularGameplayActors",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				// Energy cycle 2: UAFLW_EnergyMeter (the HUD meter's C++ binding half -- BP graph
				// authoring is bridge-hostile, so the delegate work lives in code).
				"UMG",
				"Niagara",
				// AFL-0208: IAFLBeamEndpointProvider, implemented by UAFLBeamChannelComponent.
				// AFLVFX is the always-on root cosmetic plugin that owns the beam-cue contract;
				// AFLCombat (a GameFeature) depending on it is the correct load-order direction.
				// Private: only AFLCombat's own TUs compile the component that implements it.
				"AFLVFX",
				// Loot Phase 3: AAFLLootCacheCarry wears UAFLGrabbableComponent (AFLMovement). NON-CIRCULAR (AFLMovement !-> AFLCombat).
				"AFLMovement",
				// AFL death system (1b): AAFLTargetDummy subclasses ALyraCharacter, which
				// implements IGenericTeamAgentInterface (FGenericTeamId / team-attitude). Those
				// symbols live in AIModule -- LyraGame links it for the same reason
				// (LyraGame.Build.cs). Without it: LNK2019 on the inherited interface thunks.
				"AIModule",
				// S-ECON-STORE: afl.Store.Open pushes the cosmetic-store ULyraActivatableWidget onto
				// the UI.Layer.Menu CommonUI layer via UCommonUIExtensions::PushContentToLayer_ForPlayer.
				// UCommonUIExtensions lives in CommonGame; the layer/activatable types in CommonUI.
				// LyraGame (public dep) uses both for the same push; AFLCombat needs them directly to
				// compile the include. GameFeature -> engine-plugin module direction is correct.
				"CommonUI",
				"CommonGame",
				// EOS-AUTH-C2: the afl.EOS.Auth.Status / afl.EOS.Friends.Query cheats read the OSSv2
				// UE::Online path -- GetServices(EOnlineServices::Epic), IAuth::GetLocalOnlineUserBy
				// PlatformUserId, ISocial::QueryFriends/GetFriends. The IOnlineServices/IAuth/ISocial
				// interfaces live in OnlineServicesInterface; FAccountId + ToLogString(FAccountId) in
				// CoreOnline. Private: only AFLCombatCheats.cpp compiles them. EAS auth/friends only --
				// matchmaking stays PlayFab->GameLift (the tentpole, separate repo).
				"OnlineServicesInterface",
				"CoreOnline",
				// A1.1 economy persistence PlayFab LOAD path: UAFLOnlineSubsystem (PlayFab client login +
				// the thin REST transport) + FJsonObject parsing of the GetUserInventory response. Private:
				// only the persistence subsystem + the two MakePlayerId bodies compile them. GameFeature ->
				// always-loaded AFLOnline is the correct load-order direction (login resident before BeginPlay).
				"AFLOnline",
				"Json",
			}
		);
	}
}

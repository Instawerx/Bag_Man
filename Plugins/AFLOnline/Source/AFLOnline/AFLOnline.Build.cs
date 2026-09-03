// Copyright C12 AI Gaming. All Rights Reserved.

using UnrealBuildTool;

public class AFLOnline : ModuleRules
{
	public AFLOnline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				// The public header exposes FHttpRequestPtr/FHttpResponsePtr in a callback signature.
				"HTTP",
				// COMMS-1 text chat: FAFLChatMessage / EAFLChatChannel are public in the chat headers, so the
				// always-loaded types plugin is a PUBLIC dep. FUniqueNetIdRepl comes from Engine (above).
				"AFLNetTypes",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// FJsonObject / FJsonSerializer -- PlayFab REST request/response bodies.
				"Json",

				// COMMS-5 DM client (AFLSocialSubsystem): IWebSocket / FWebSocketsModule for the persistent
				// DM WebSocket transport (used only in the social .cpp -- the header forward-declares IWebSocket).
				"WebSockets",


				// COMMS-1 text chat spine (private -- used only in the chat .cpp files):
				"NetCore",         // replication plumbing for the owner-only chat RPCs
				"ModularGameplay", // UGameFrameworkComponentManager::AddComponentRequest (attach the chat component)
				"AIModule",        // IGenericTeamAgentInterface -- Team-channel same-team resolution (no LyraGame dep)

				// D17 SHIPPING LOGIN (EOS Default). The shipping PlayFab login is
				// LoginWithOpenIdConnect against an Epic Account Services ID token, so this module needs
				// the EOS SDK directly:
				//   EOSShared -> IEOSSDKManager, the ONLY public way to reach the live EOS_HPlatform
				//                (OnlineSubsystemEOS's own FUserManagerEOS is Private and unexported, and
				//                 its GetAuthToken returns the ACCESS token -- the wrong artifact here).
				//   EOSSDK    -> eos_auth.h + EOS_Auth_CopyIdToken, and it PUBLICLY defines WITH_EOS_SDK=1,
				//                which is what gates every EOS block in the .cpp. On a platform where the
				//                SDK is unavailable the define is absent, those blocks compile out, and the
				//                shipping path fails CLOSED with a diagnostic rather than silently.
				"EOSShared",
				"EOSSDK",
			}
		);

		// COMMS-3/4 voice: the client-side IVoiceChat wiring lives in UAFLVoiceSubsystem, compiled only when
		// AFLONLINE_WITH_VOICE. The two EOS voice plugins (VoiceChat interface + EOSVoiceChat) are enabled in
		// Bag_Man.uproject and compile NATIVELY on the D: source engine (Docs/ENGINE_DOCTRINE.md) -- the retired
		// binary launcher could not, because VoiceChat is a header-only External module with no precompiled
		// binary ("'VoiceChat' is not a C++ module"). GUARD-FLIP GATE (COMMS-3): set bAFLVoice=true to activate
		// the subsystem and pull the deps below. Stays false for the foundation commit (guard-off build gate).
		bool bAFLVoice = false;
		PublicDefinitions.Add("AFLONLINE_WITH_VOICE=" + (bAFLVoice ? "1" : "0"));
		if (bAFLVoice)
		{
			// VoiceChat: header-only IVoiceChat interface (External module), cross-platform.
			PrivateDependencyModuleNames.Add("VoiceChat");
			// EOSVoiceChat: EOS implementation (ClientOnlyNoCommandlet; .uplugin allowlist Win64/Mac/Android).
			// Link it only where the plugin exists; the subsystem uses IVoiceChat::Get() (modular feature)
			// elsewhere so no unsupported platform (e.g. iOS) takes a hard link dep.
			if (Target.Platform == UnrealTargetPlatform.Win64
				|| Target.Platform == UnrealTargetPlatform.Mac
				|| Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.Add("EOSVoiceChat");
			}
		}

		// A1.3b earn signer: OpenSSL HMAC-SHA256 for the server-authoritative /earn request signature. Mirror
		// PlatformCryptoContext's mechanism -- a module-private define gates the <openssl/*> include so the signer
		// compiles clean on every platform and only RUNS under IsRunningDedicatedServer() (the KEY is env-only,
		// never cooked). The angle-bracket openssl headers come from the OpenSSL ThirdParty dependency.
		PrivateDefinitions.Add("AFLONLINE_USE_OPENSSL=1");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
	}
}
